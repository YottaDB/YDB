 /****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <stdarg.h>
#include "gtm_stdio.h"
#include "gtm_syslog.h"
#include <errno.h>

#include "gtm_multi_thread.h"
#include "io.h"
#include "error.h"
#include "fao_parm.h"
#include "min_max.h"
#include "hashtab_mname.h"
#include "util.h"
#include "util_format.h"
#include "util_out_print_vaparm.h"
#include "gtmimagename.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "repl_shutdcode.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "repl_instance.h"
#include "ydb_trans_log_name.h"
#include "gtmio.h"
#include "have_crit.h"
#include "gtm_multi_proc.h"
#include "get_syslog_flags.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

GBLDEF	boolean_t		first_syslog = TRUE;	/* Global for a process - not thread specific */
GBLDEF	char			facility[MAX_INSTNAME_LEN + 100];
#ifdef DEBUG
GBLDEF	boolean_t		util_out_print_vaparm_flush;
#endif

GBLREF	io_pair			io_std_device;
GBLREF	boolean_t		blocksig_initialized;
GBLREF	sigset_t		block_sigsent;
GBLREF	boolean_t		err_same_as_out;
GBLREF	gd_addr			*gd_header;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		is_rcvr_server;
GBLREF	boolean_t		is_updproc;
GBLREF	uint4			is_updhelper;
GBLREF	recvpool_addrs		recvpool;
GBLREF	uint4			process_id;
GBLREF	VSIG_ATOMIC_T		forced_exit;

error_def(ERR_REPLINSTACC);
error_def(ERR_TEXT);

#define	ZTRIGBUFF_INIT_ALLOC		1024	/* start at 1K */
#define	ZTRIGBUFF_INIT_MAX_GEOM_ALLOC	1048576	/* stop geometric growth at this value */

/* #GTM_THREAD_SAFE : The below macro (GETFAOVALDEF) is thread-safe because caller ensures serialization with locks */
#define GETFAOVALDEF(faocnt, var, type, result, defval)		\
{								\
	assert(IS_PTHREAD_LOCKED_AND_HOLDER);			\
	if (faocnt > 0)						\
	{							\
		result = (type)va_arg(var, type);		\
		faocnt--;					\
	} else							\
		result = defval;				\
}

/* #GTM_THREAD_SAFE : The below macro (INSERT_MARKER) is thread-safe because caller ensures serialization with locks */
#define INSERT_MARKER						\
{								\
	assert(IS_PTHREAD_LOCKED_AND_HOLDER);			\
	MEMCPY_LIT(offset, "-");				\
	offset += STRLEN("-");					\
}

/* #GTM_THREAD_SAFE : The below macro (BUILD_FACILITY) is thread-safe because caller ensures serialization with locks */
#define BUILD_FACILITY(strptr)					\
{								\
	assert(IS_PTHREAD_LOCKED_AND_HOLDER);			\
	memcpy(offset, strptr, STRLEN(strptr));			\
	offset += STRLEN(strptr);				\
}

/*
 *	This routine implements a SUBSET of FAO directives, namely:
 *
 *		!/	!_	!^	!!
 *
 *
 *		!mAC	!mAD	!mAF	!mAS	!mAZ
 *
 *		!mSB	!mSW	!mSL
 *
 *		!mUB	!mUW	!mUL    !m@UJ   !m@UQ
 *
 *		!mXB	!mXW	!mXL    !mXJ    !m@XJ   !m@XQ
 *
 *		!mZB	!mZW	!mZL
 *
 *		!n*c
 *
 *		!@ZJ	!@XJ		!@ZJ	!@ZQ
 #
 *	Where `m' is an optional field width, `n' is a repeat count, and `c' is a single character.
 *	`m' or `n' may be specified as the '#' character, in which case the value is taken from the next parameter.
 *
 *	FAO stands for "formatted ASCII output". The FAO directives may be considered equivalent to format
 *	specifications and are documented with the VMS Lexical Fuction F$FAO in the OpenVMS DCL Dictionary.
 *
 *	The @XH and @XJ types need special mention. XH and XJ are ascii formatting of addresses and integers respectively. BOTH are
 *	ASCII formatted hexdecimal output of a 64 bit sign-extended value. The present implementation of util_output does not
 *	support 'H'. This support was new in VMS 7.2 (and is one reason why GTM 4.2 requires VMS 7.2). The "@" designates an
 *	"indirect" request meaning that the address of the 8 byte item is passed rather than the item itself. This is what allows
 *	us to print 8 byte values in the non-Alpha 32 bit parameter worlds. These types are documented in the VMS System services
 *	manual under SYS$FAO. There are several other types that are supported on VMS but only these two were added on Unix.
 *
 *	Another variant of the 'J' type is !mXJ which the routine implements. This variant is used to print 'addresses' in platform
 *	independent way. For examples of this type, see the definition and usages of the following messages:
 *
 *	CALLERID
 *	KILLBYSIGSINFO1
 *	KILLBYSIGSINFO2
 *
 *	One important caveat in using !mXJ variant is that the input value is expected to be 4-byte on 32-bit platforms and 8-byte
 *	on 64-bit platforms. Passing an 8-byte quantity on a 32-bit platform can cause SIGSEGV. If a field is always 8-bytes on both
 * 	the 32 and 64 bit platforms (like transaction numbers), use 0x!16@XQ variant instead.
 *
 *	In addition, this routine also implements another set of directives
 *
 *		!RmAC	!RmAD	!RmAF	!RmAS	!RmAZ
 *
 *	This implements the !mAx equivalent but does right-justification of the string instead of left-justification.
 */

/*
 *	util_format - convert FAO format string to C PRINTF format string.
 *
 *	input arguments:
 *		message	- one of the message strings from, for example, merrors.c
 *		fao	- list of values to be inserted into message according to
 *			  the FAO directives
 *		size	- size of buff
 *
 *	output argument:
 *		buff	- will contain C PRINTF-style format statement with any
 *			  "A" (character) fields filled in from fao list
 *
 *	output global value:
 *		outparm[] - array of numeric arguments from fao list (character
 *			    arguments already incorporated into buff
 *
 */
/* #GTM_THREAD_SAFE : The below function (util_format) is thread-safe because caller ensures serialization with locks */
caddr_t util_format(caddr_t message, va_list fao, caddr_t buff, ssize_t size, int faocnt)
{
	desc_struct	*d;
	signed char	schar;
	unsigned char	type, type2;
	caddr_t		c, ctop, outptr, outtop, outtop1, message_next, message_top;
	uchar_ptr_t 	ret_ptr;
	unsigned char	uchar;
	short		sshort, *s;
	unsigned short	ushort;
	int		i, nexti, length, field_width, repeat_count, int_val, chwidth, orig_chwidth, cwidth;
	unsigned int	ch;
	UINTPTR_T	addr_val;
	ssize_t		chlen;
	boolean_t	indirect;
	qw_num_ptr_t	val_ptr;
	unsigned char	numa[22];
	unsigned char	*numptr, *prefix;
	boolean_t	right_justify, isprintable, line_begin;
	int		prefix_len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(IS_PTHREAD_LOCKED_AND_HOLDER);
	VAR_COPY(TREF(last_va_list_ptr), fao);
	if (multi_proc_in_use)
	{
		/* If we are about to flush the output to stdout/stderr, we better hold a latch (since we dont want
		 * parallel processes mixing their outputs in the same device. The only exception is if caller
		 * "util_out_print_vaparm" has been called without "FLUSH" argument which means the output goes to
		 * a string or syslog or unflushed buffer. It is ok to not hold the latch in those cases. Assert that.
		 */
		assert((process_id == multi_proc_shm_hdr->multi_proc_latch.u.parts.latch_pid)
			|| (FLUSH != util_out_print_vaparm_flush));
		/* By similar reasoning as the above assert, assert on the key prefix */
		assert((NULL != multi_proc_key) || multi_proc_key_exception || (FLUSH != util_out_print_vaparm_flush));
		prefix = multi_proc_key;
		prefix_len = (NULL == prefix) ? 0 : STRLEN((char *)prefix);
		line_begin = (TREF(util_outptr) == TREF(util_outbuff_ptr));
	} else
		prefix = NULL;
	outptr = buff;
	outtop = outptr + size - 5;	/* 5 bytes to prevent writing across border */
	while (outptr < outtop)
	{
		if ((NULL != prefix) && line_begin)
		{
			if ((outptr + prefix_len + 3) >= outtop)
				break;
			memcpy(outptr, prefix, prefix_len);
			outptr += prefix_len;
			*outptr++ = ' ';
			*outptr++ = ':';
			*outptr++ = ' ';
			line_begin = FALSE;
		}
		/* Look for the '!' that starts an FAO directive */
		while ((schar = *message++) != '!')
		{
			if (schar == '\0')
			{
				va_end(TREF(last_va_list_ptr));	/* reset before using as dest in copy */
				VAR_COPY(TREF(last_va_list_ptr), fao);
				return outptr;
			}
			*outptr++ = schar;
			if (outptr >= outtop)
			{
				va_end(TREF(last_va_list_ptr));	/* reset before using as dest in copy */
				VAR_COPY(TREF(last_va_list_ptr), fao);
				return outptr;
			}
		}
		field_width = 0;	/* Default values */
		repeat_count = 1;
		right_justify = FALSE;
		if ('R' == *message)
		{
			right_justify = TRUE;
			++message;
		}
		/* Look for a field width (or repeat count) */
		if (*message == '#')
		{
			if (0 < faocnt)
				field_width = repeat_count = va_arg(fao, int4);
			++message;
		} else
		{
			for (c = message;  *c >= '0'  &&  *c <= '9';  ++c)
				;
			if ((length = (int)(c - message)) > 0)
			{
				field_width = repeat_count
					= asc2i((uchar_ptr_t)message, length);
				message = c;
			}
		}
		if ('@' == *message)			/* Indirectly addressed operand */
		{
			indirect = TRUE;
			message++;
		} else
			indirect = FALSE;
		switch (type = *message++)
		{
			case '/':
				assert(!indirect);
				*outptr++ = '\n';
				line_begin = TRUE;
				continue;
			case '_':
				assert(!indirect);
				*outptr++ = '\t';
				continue;
			case '^':
				assert(!indirect);
				*outptr++ = '\f';
				continue;
			case '!':
				assert(!indirect);
				*outptr++ = '!';
				continue;
			case '*':
				assert(!indirect);
				if (repeat_count > 0)
				{
					message_top = message + strlen(message);
					assert(message < message_top);
					chlen = (!gtm_utf8_mode) ? 1 :
						((caddr_t)UTF8_MBNEXT(message, message_top) - message);
				} else
					chlen = 0;
				while ((repeat_count-- > 0) && (outptr < outtop))
				{
					memcpy(outptr, message, chlen);
					outptr += chlen;
				}
				message += chlen;
				continue;
			case 'A':
				assert(!indirect);
				switch(type2 = *message++)
				{
					case 'C': /* a string with length in the first byte */
						GETFAOVALDEF(faocnt, fao, caddr_t, c, NULL);
						length = c ? *c++ : 0;
						break;
					case 'D':
					case 'F': /* string with length and addr parameters */
						GETFAOVALDEF(faocnt, fao, int4, length, 0);
						GETFAOVALDEF(faocnt, fao, caddr_t, c, NULL);
						break;
					case 'S':
						if (faocnt)
						{
							d = (desc_struct *)va_arg(fao, caddr_t);
							faocnt--;
							c = d->addr;
							length = d->len;
						} else
						{
							c = NULL;
							length = 0;
						}
						break;
					case 'Z': /* null teminated string */
						GETFAOVALDEF(faocnt, fao, caddr_t, c, NULL);
						length = c ? STRLEN(c) : 0;
						break;
					default:
						assert(FALSE);
						length = MAXINT4;	/* For static scan */
				}
				/* Since gtmsecshr does not load ICU libraries (since dlopen() with LD_LIBRARY_PATH
				 * does not work for root setuid executables), avoid calling gtm_wcswidth() and
				 * U_ISPRINT() from gtmsecshr and thus non-zero widths used in util_out_print()
				 * from gtmsecshr will not be treated as column widths but as character lengths.
				 * This is a safe limitation since no message from gtmsecshr specifies width yet.
				 */
				assert(!gtm_utf8_mode || IS_GTMSECSHR_IMAGE || (NULL != gtm_wcswidth_fnptr));
				if (MAXINT4 != length)
				{	/* Block protected from static scan in case length is not valid */
					cwidth = (!gtm_utf8_mode || IS_GTMSECSHR_IMAGE)
						? length : (*gtm_wcswidth_fnptr)((unsigned char *)c, length, FALSE, 1);
					if ((0 < field_width) && (cwidth > field_width))
						cwidth = field_width;
					assert(0 <= cwidth); /* since all unprintable and illegal characters are ignored */
				}
				assert(0 <= field_width);
				outtop1 = outtop - 1;
				if (right_justify)
				{
					for (i = field_width - cwidth;  i > 0 && outptr < outtop1; --i)
						*outptr++ = ' ';
				}
				if (!gtm_utf8_mode)
				{
					chwidth = 1;	/* for both printable and unprintable characters */
					chlen = 1;
				}
				for (i = 0, ctop = c + length; c < ctop; c += chlen)
				{
					if (!gtm_utf8_mode)
					{
						ch = *c;
						isprintable = ((' ' <= ch) || ('~' >= ch)); /* Ignored in M mode for FAO !AD */
					} else
					{
						chlen = (caddr_t)UTF8_MBTOWC(c, ctop, ch) - c;
						if (!IS_GTMSECSHR_IMAGE)
						{
							chwidth = (int)UTF8_WCWIDTH(ch);
							/* Note down chwidth (for debugging) from ICU before tampering with it */
							DEBUG_ONLY(orig_chwidth = chwidth;)
							if (-1 != chwidth)
								isprintable = TRUE;
							else
							{
								isprintable = U_ISSPACE(ch);
								chwidth = 1; /* treat unprintable characters as having width=1 */
							}
						} else
						{	/* Assume printability for GTMSECSHR */
							chwidth = (int)chlen;
							isprintable = TRUE;
						}
					}
					assert(('\0' != ch) || (2 != faocnt) || !STRNCMP_LIT(message, "!/!_!AD"));
					/* the assert above is to detect <NUL> bytes in GT.M messages, which cause trouble;
					 * however, we have no control over the content of user-supplied input: that may contain
					 * <NUL> bytes, which, as of this writing, we believe is confined, at least in our testing,
					 * to reporting source syntax errors; therefore the assert tries to side-step any such
					 * syntax errors by their signature, which is unique to the messages for SRCLINE and
					 * EXTSRCLIN and. although DBG only, avoids a GBLDEF or gtm_threadglb_def
					 */
					assert((c + chlen) <= ctop);
					assert(0 < chlen);
					assert((0 < chwidth) || (0 == chwidth) && gtm_utf8_mode);
					nexti = i + chwidth;
					if (nexti > cwidth)	/* adding next input char will cross requested width */
						break;
					if ((outptr + chlen) > outtop1)	/* adding next input char will cross output buffer limit */
						break;
					if (!isprintable && (('F' == type2) UNICODE_ONLY(|| (('D' == type2) && gtm_utf8_mode))))
					{	/* Since HPUX stops printing lines (via FPRINTF) when it
						 * encounters a bad character, all platforms in utf8 mode
						 * will behave as if !AF were specified and put a "." in place
						 * of non-printable characters. SE 01/2007
						 */
						*outptr++ = '.';
						i = nexti;
					} else if ('\0' != ch)	/* skip NULL bytes in the middle of the string */
					{
						if (1 == chlen)
							*outptr++ = *c;
						else
						{
							memcpy(outptr, c, chlen);
							outptr += chlen;
						}
						i = nexti;
					}
				}
				/* Ensure we are still within limits */
				assert(outptr <= outtop1);
				assert(i <= cwidth);
				assert(c <= ctop);
				if (!right_justify)
				{
					for (i = field_width - i;  i > 0 && outptr < outtop1;  --i)
						*outptr++ = ' ';
				}
				continue;
			default:	/* Rest of numeric types come here */
				assert('S' == type || 'U' == type || 'X' == type || 'Z' == type);
				numptr = numa;
				type2 = *message++;
				if (!indirect)
				{
					if ('S' == type)
						switch(type2)
						{
							case 'B':
								GETFAOVALDEF(faocnt, fao, int4, schar, 0);
								int_val = schar;
								break;
							case 'W':
								GETFAOVALDEF(faocnt, fao, int4, sshort, 0);
								int_val = sshort;
								break;
							case 'L':
								GETFAOVALDEF(faocnt, fao, int4, int_val, 0);
								break;
							case 'J':
								GTM64_ONLY(
								GETFAOVALDEF(faocnt, fao, UINTPTR_T, addr_val, 0);
								)
								NON_GTM64_ONLY(
								GETFAOVALDEF(faocnt, fao, int4, int_val, 0);
								)
								break;
							default:
								assert(FALSE);
						}
					else
					{
						GTM64_ONLY(
						if ('J' == type2)
							{GETFAOVALDEF(faocnt, fao, UINTPTR_T, addr_val, 0);}
						else
							{GETFAOVALDEF(faocnt, fao, int4, int_val, 0);}
						)
						NON_GTM64_ONLY(GETFAOVALDEF(faocnt, fao, int4, int_val, 0);)
						switch(type2)
						{
							case 'B':
								int_val = int_val & 0xFF;
								break;
							case 'W':
								int_val = int_val & 0xFFFF;
								break;
							case 'L':
								int_val = int_val & 0xFFFFFFFF;
								break;
							case 'J':
								NON_GTM64_ONLY(int_val = int_val & 0xFFFFFFFF;)
								break;
							default:
								assert(FALSE);
						}
					}
					switch (type)
					{
						case 'S':	/* Signed value. Give sign if need to */
							if ('J' == type2)
							{
								GTM64_ONLY(
									if (0 > (INTPTR_T)addr_val)
									{
										*numptr++ = '-';
										addr_val = -(addr_val);
									}
								)
								NON_GTM64_ONLY(
									if (0 > int_val)
									{
										*numptr++ = '-';
										int_val = -(int_val);
									}
								)
							} else if (0 > int_val)
							{
								*numptr++ = '-';
								int_val = -(int_val);
							}	/* note fall into unsigned */
						case 'U':
						case 'Z':		/* zero filled */
							NON_GTM64_ONLY(numptr = i2asc(numptr, int_val);)
							GTM64_ONLY(
							if ('J' == type2)
								numptr = i2ascl(numptr, addr_val);
							else
								numptr = i2asc(numptr, int_val);
							)
							break;
						case 'X':		/* Hex */
							switch (type2)
							{	/* length is number of ascii hex chars */
								case 'B':
							        	length = SIZEOF(short);
							         	break;
								case 'W':
									length = SIZEOF(int4);
							                break;
								case 'L':
									length = 2 * SIZEOF(int4);
							                break;
							        case 'J':
									length = 2 * SIZEOF(INTPTR_T);
						                       	break;
								default:
									assert(FALSE);
							}
							NON_GTM64_ONLY(i2hex(int_val, numptr, length);)
							GTM64_ONLY(i2hex(('J' == type2) ? addr_val : int_val, numptr, length);)

							numptr += length;
							break;
						default:
							assert(FALSE);
					}
				} else
				{
					if ('X' == type)	/* Support XJ and XQ */
					{
						assert('J' == type2 || 'Q' == type2);
						GETFAOVALDEF(faocnt, fao, qw_num_ptr_t, val_ptr, NULL);	/* Addr of long type */
						if (val_ptr)
						{
							if (0 != field_width)
							{
								i2hexl(*val_ptr, numptr, field_width);
								numptr += field_width;
							} else
							{
								length = i2hexl_nofill(*val_ptr, numptr, HEX16);
								numptr += length;
							}
						}
					} else 	/* support ZJ, ZQ, UQ and UJ */
					{
						assertpro(('Z' == type) || ('U' == type));
						assert('J' == type2 || 'Q' == type2);
						GETFAOVALDEF(faocnt, fao, qw_num_ptr_t, val_ptr, NULL);	/* Addr of long type */
						if (val_ptr)
						{
							ret_ptr = i2ascl(numptr, *val_ptr);
							length =(int)(ret_ptr - (uchar_ptr_t)numptr);
							if (0 != field_width)
								numptr += MIN(length, field_width);
							else
								numptr += length;
						}
					}
				}
				length = (int)(numptr - numa);		/* Length of asciified number */
				if (length < field_width)
				{
					memset(outptr, (('Z' == type) ? '0' : ' '), field_width - length);
					outptr += field_width - length;
				}
				if ((field_width > 0) && (field_width < length))
				{
					GTM64_ONLY(
					/* If this is an integer to be printed using format specifier X, display the
					   least 4 bytes */
					if (type == 'X' && type2 == 'J' && (length == (2 * SIZEOF(INTPTR_T))))
						memcpy(outptr, numa + SIZEOF(INTPTR_T), length/2);
					else
						memset(outptr, '*', field_width);
					)
					NON_GTM64_ONLY(memset(outptr, '*', field_width);)
					outptr += field_width;
				} else
				{
					memcpy(outptr, numa, length);
					outptr += length;
				}
		}
	}
	va_end(TREF(last_va_list_ptr));	/* reset before using as dest in copy */
	VAR_COPY(TREF(last_va_list_ptr), fao);
	return outptr;
}

/* #GTM_THREAD_SAFE : The below function (util_format) is thread-safe because caller ensures serialization with locks */
void	util_out_close(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ASSERT_SAFE_TO_UPDATE_THREAD_GBLS;
	if ((NULL != TREF(util_outptr)) && (TREF(util_outptr) != TREF(util_outbuff_ptr)))
		util_out_print("", FLUSH);
}

/* #GTM_THREAD_SAFE : The below function (util_out_send_oper) is thread-safe because caller ensures serialization with locks */
void	util_out_send_oper(char *addr, unsigned int len)
/* 1st arg: address of system log message */
/* 2nd arg: length of system long message (not used in Unix implementation) */
{
	sigset_t		savemask;
	char			*img_type, *offset;
	char 			temp_inst_fn[MAX_FN_LEN + 1], fn[MAX_FN_LEN + 1];
	mstr			log_nam, trans_name;
	uint4			ustatus;
	int4			status;
	unsigned int		bufsize, file_name_len, *fn_len;
	boolean_t		ret, inst_from_gld;
	repl_inst_hdr		replhdr;
	int			fd;
	upd_helper_ctl_ptr_t	upd_helper_ctl;
	upd_helper_entry_ptr_t	helper, helper_top;
	intrpt_state_t		prev_intrpt_state;

	assert(IS_PTHREAD_LOCKED_AND_HOLDER);
	if (first_syslog)
	{
		first_syslog = FALSE;

		offset = facility;
		BUILD_FACILITY("YDB");
		INSERT_MARKER;
		switch (image_type)
		{
			case GTM_IMAGE:
				img_type = "MUMPS";
				break;
			case MUPIP_IMAGE:
				if (is_src_server)
					img_type = "SRCSRVR";
				else if (is_rcvr_server)
					img_type = "RCVSRVR";
				else if (is_updproc)
					img_type = "UPDPROC";
				else if (is_updhelper)
				{
					assert((UPD_HELPER_READER == is_updhelper) || (UPD_HELPER_WRITER == is_updhelper));
					if (UPD_HELPER_READER == is_updhelper)
						img_type = "UPDREAD";
					else
						img_type = "UPDWRITE";
				} else
					img_type = "MUPIP";
				break;
			case DSE_IMAGE:
				img_type = "DSE";
				break;
			case LKE_IMAGE:
				img_type = "LKE";
				break;
			case DBCERTIFY_IMAGE:
				img_type = "DBCERTIFY";
				break;
			case GTCM_SERVER_IMAGE:
				img_type = "GTCM";
				break;
			case GTCM_GNP_SERVER_IMAGE:
				img_type = "GTCM_GNP";
				break;
			case GTMSECSHR_IMAGE:
				img_type = "SECSHR";
				break;
			default:
				assertpro(FALSE);
		}
		BUILD_FACILITY(img_type);
		if ((NULL != jnlpool) && (NULL != jnlpool->jnlpool_ctl) && (NULL != jnlpool->repl_inst_filehdr))
		{	/* Read instace file name from jnlpool */
			INSERT_MARKER;
			BUILD_FACILITY((char *)jnlpool->repl_inst_filehdr->inst_info.this_instname);
		} else
		{	/* Read instance name from instance file */
			fn_len = &file_name_len;
			bufsize = MAX_FN_LEN + 1;
			SETUP_INST_INFO(gd_header, log_nam, inst_from_gld);	/* set log_nam from GLD or environment variable */
			trans_name.addr = temp_inst_fn;
			ret = FALSE;
			GET_INSTFILE_NAME(dont_sendmsg_on_log2long, return_on_error);
			/* We want the instance name as part of operator log messages, but if we cannot get it,
			 * we will get by without it, so ignore any errors we might encounter trying to find the name
			 */
			if (ret)
			{
				OPENFILE(fn, O_RDONLY, fd);
				if (FD_INVALID != fd)
				{
					LSEEKREAD(fd, 0, &replhdr, SIZEOF(repl_inst_hdr), status);
					if (0 == status)
					{
						INSERT_MARKER;
						BUILD_FACILITY((char *)replhdr.inst_info.this_instname);
					}
					CLOSEFILE_RESET(fd, status);
				}
			}
		}
		DEFER_INTERRUPTS(INTRPT_IN_LOG_FUNCTION, prev_intrpt_state);
		(void)OPENLOG(facility, get_syslog_flags(), LOG_USER);
		ENABLE_INTERRUPTS(INTRPT_IN_LOG_FUNCTION, prev_intrpt_state);
	}
	/* When syslog is processing and a signal occurs, the signal processing might eventually lead to another syslog
	 * call. But in libc the first syslog has grabbed a lock (syslog_lock), and now the other syslog call will
	 * block waiting for that lock which can't be released since the first syslog was interrupted by the signal.
	 * We address this issue by deferring signals for the duration of the call; generic_signal_handler.c will also
	 * skip send_msg invocations if the interrupt comes while INTRPT_IN_LOG_FUNCTION is set.
	 */
	DEFER_INTERRUPTS(INTRPT_IN_LOG_FUNCTION, prev_intrpt_state);
	SYSLOG(LOG_USER | LOG_INFO, "%s", addr);
	ENABLE_INTERRUPTS(INTRPT_IN_LOG_FUNCTION, prev_intrpt_state);
}

/* #GTM_THREAD_SAFE : The below function (util_out_print_vaparm) is thread-safe because caller ensures serialization with locks */
void	util_out_print_vaparm(caddr_t message, int flush, va_list var, int faocnt)
{
	char		fmt_buff[OUT_BUFF_SIZE];	/* needs to be same size as that of the util out buffer */
	caddr_t		fmtc;
	int		rc, count;
	char		*fmt_top1, *fmt_top2; /* the top of the buffer after leaving 1 (and 2 bytes respectively) at the end */
	int		util_avail_len;
	mstr		flushtxt;
	boolean_t	use_gtmio;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (IS_GTMSECSHR_IMAGE && (FLUSH == flush))
		flush = OPER;			/* All gtmsecshr origin msgs go to operator log */
	ASSERT_SAFE_TO_UPDATE_THREAD_GBLS;
	assert(NULL != TREF(util_outptr));
	if (NULL != message)
	{
		util_avail_len = INTCAST(TREF(util_outbuff_ptr) + OUT_BUFF_SIZE - TREF(util_outptr) - 2);
		assert(0 <= util_avail_len);
		if (0 < util_avail_len)
		{
			DEBUG_ONLY(util_out_print_vaparm_flush = flush;)
			TREF(util_outptr) = util_format(message, var, TREF(util_outptr), util_avail_len, faocnt);
			DEBUG_ONLY(util_out_print_vaparm_flush = FLUSH;)
		}
	}
	use_gtmio = ((NULL != io_std_device.out) && (!IS_GTMSECSHR_IMAGE) && err_same_as_out);
	switch (flush)
	{
		case NOFLUSH:
			break;
		case RESET:
			break;
		case FLUSH:
			if (!use_gtmio)
				*(TREF(util_outptr))++ = '\n';
		case OPER:
		case SPRINT:
			/* For all three of these actions we need to do some output buffer translation. In all cases a '%'
			 * is translated to the escape version '%%'. For OPER and SPRINT, we also translate '\n' to a ', '
			 * since some syslog() implementations (like Tru64) stop processing the passed message on a newline.
			 * Note that since the '%' -> '%%' or '\n' to ', ' translations imply an expansion in the buffer size
			 * requirements, we could potentially overflow the buffer after the translation. In that case we will
			 * stop copying just before the point of overflow is reached even though it means loss of the tail data.
			 */
			*(TREF(util_outptr)) = '\0';
			fmt_top1 = fmt_buff + SIZEOF(fmt_buff) - 1;
			fmt_top2 = fmt_top1 - 1;
			for (TREF(util_outptr) = TREF(util_outbuff_ptr), fmtc = fmt_buff;
						(0 != *(TREF(util_outptr))) && (fmtc < fmt_top1); )
			{
				if ('%' == *(TREF(util_outptr)))
				{
					if (fmtc >= fmt_top2) /* Check if there is room for 2 bytes. If not stop copying */
						break;
					if (flush == SPRINT)
						*fmtc++ = '%'; /* give buffered users what they expect %% */
					*fmtc++ = '%';
					(TREF(util_outptr))++;
				} else if ('\n' == *(TREF(util_outptr)) && (OPER == flush || SPRINT == flush))
				{
					if (fmtc >= fmt_top2) /* Check if there is room for 2 bytes. If not stop copying */
						break;
					*fmtc++ = ',';
					*fmtc++ = ' ';
					(TREF(util_outptr))++;
				} else
					*fmtc++ = *(TREF(util_outptr))++;
			}
			assert(fmtc <= fmt_top1);
			*fmtc++ = '\0';
			switch (flush)
			{
				case FLUSH:
					/* Before flushing something to a file/terminal, check if parent is still alive.
					 * If not, return without flushing. This child will eventually return when it
					 * does a IS_FORCED_MULTI_PROC_EXIT check at a logical point.
					 */
					if (multi_proc_in_use && (process_id != multi_proc_shm_hdr->parent_pid)
							&& (getppid() != multi_proc_shm_hdr->parent_pid))
					{
						SET_FORCED_MULTI_PROC_EXIT; /* Also signal sibling children to stop processing */
						return;
					}
					if (err_same_as_out)
					{	/* If err and out are conjoined, make sure that all messages that might have been
						 * printed using PRINTF (unfortunately, we still have lots of such instances) are
						 * flushed before we proceed.
						 */
						FFLUSH(stdout);
						FFLUSH(stderr);
					}
					if (use_gtmio)
					{
						flushtxt.addr = fmt_buff;
						flushtxt.len = INTCAST(TREF(util_outptr) - TREF(util_outbuff_ptr));
						write_text_newline_and_flush_pio(&flushtxt);
					} else
					{	/* We should start caring about FPRINTF's return status at some point. */
						FPRINTF(stderr, "%s", fmt_buff);
						FFLUSH(stderr);
					}
					break;
				case OPER:
					util_out_send_oper(fmt_buff, UINTCAST(fmtc - fmt_buff));
					break;
				case SPRINT:
					memcpy(TREF(util_outbuff_ptr), fmt_buff, fmtc - fmt_buff);
					break;
			}
			break;
		default:
			assert(FALSE);
	}
	switch (flush)
	{
		case NOFLUSH:
			break;
		case FLUSH:
		case RESET:
		case OPER:
		case SPRINT:
			/* Reset buffer information */
			TREF(util_outptr) = TREF(util_outbuff_ptr);
			break;
	}
}

/* #GTM_THREAD_SAFE : The below function (util_out_print) is thread-safe because caller ensures serialization with locks */
void	util_out_print(caddr_t message, int flush, ...)
{
	va_list	var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ASSERT_SAFE_TO_UPDATE_THREAD_GBLS;
	va_start(var, flush);
	util_out_print_vaparm(message, flush, var, MAXPOSINT4);
	va_end(TREF(last_va_list_ptr));
	va_end(var);
}

/* Saves a copy of the current unflushed buffer in util_outbuff_ptr into dst.
 * The length of the allocated buffer at "dst" is passed in as *dst_len.
 * If that length is not enough to store the unflushed buffer, FALSE is returned right away.
 * If not, the length of the unflushed buffer is stored in dst_len, the actual unflushed buffer
 * is copeid over to "dst", and a TRUE is returned.
 */
boolean_t util_out_save(char *dst, int *dstlen_ptr)
{
	int	srclen, dstlen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ASSERT_SAFE_TO_UPDATE_THREAD_GBLS;
	assert(NULL != TREF(util_outptr));
	srclen = INTCAST(TREF(util_outptr) - TREF(util_outbuff_ptr));
	assert(0 <= srclen);
	assert(OUT_BUFF_SIZE >= srclen);
	dstlen = *dstlen_ptr;
	assert(0 <= dstlen);
	if (srclen > dstlen)
		return FALSE;
	*dstlen_ptr = srclen;
	memcpy(dst, TREF(util_outbuff_ptr), srclen);
	return TRUE;
}

/* #GTM_THREAD_SAFE : The below function (util_out_flush) is thread-safe because caller ensures serialization with locks */
/* If there is something in the util_outptr buffer, flush it. Called and used only by PRN_ERROR macro. */
void util_cond_flush(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ASSERT_SAFE_TO_UPDATE_THREAD_GBLS;
	if (TREF(util_outptr) != TREF(util_outbuff_ptr))
		util_out_print(NULL, FLUSH);
}

#ifdef DEBUG
/* White-box test only! Start a timer that prints something to the operator log with a period of
 * UTIL_OUT_SYSLOG_INTERVAL in attempt to interrupt util_outbuff construction and overwrite the
 * buffer's contents.
 */
void util_out_syslog_dump(void)
{
	util_out_print("Just some white-box test message long enough to ensure that "
			"whatever under-construction util_out buffer is not damaged.\n", OPER);
	/* Resubmit itself for the purposes of the white-box test which expects periodic writes to the syslog. */
	start_timer((TID)&util_out_syslog_dump, UTIL_OUT_SYSLOG_INTERVAL, util_out_syslog_dump, 0, NULL);
}
#endif
