/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

#include "io.h"
#include "error.h"
#include "fao_parm.h"
#include "min_max.h"
#include "hashtab_mname.h"
#include "util.h"
#include "util_format.h"
#include "util_out_print_vaparm.h"
#include "gtmimagename.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif


#define GETFAOVALDEF(faocnt, var, type, result, defval) \
	if (faocnt > 0) {result = (type)va_arg(var, type); faocnt--;} else result = defval;

GBLREF	io_pair			io_std_device;
GBLDEF	char		*util_outptr, util_outbuff[OUT_BUFF_SIZE];
GBLDEF	va_list		last_va_list_ptr;
GBLREF	boolean_t	blocksig_initialized;
GBLREF  sigset_t	block_sigsent;
GBLREF	void		(*op_write_ptr)(mval *v);
GBLREF	void		(*op_wteol_ptr)(int4 n);

static	boolean_t	first_syslog = TRUE;
static	char		save_util_outbuff[OUT_BUFF_SIZE];
static	int4		save_buff_used;

/*
 *	This routine implements a SUBSET of FAO directives, namely:
 *
 *		!/	!_	!^	!!
 *
 *		!mAC	!mAD	!mAF	!mAS	!mAZ
 *
 *		!mSB	!mSW	!mSL
 *
 *		!mUB	!mUW	!mUL    !m@UJ   !m@UQ
 *
 *		!mXB	!mXW	!mXL    !m@XJ   !m@XQ
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
 *	FAO stands for "formatted ASCII output".  The FAO directives may be considered equivalent to format
 *	specifications and are documented with the VMS Lexical Fuction F$FAO in the OpenVMS DCL Dictionary.
 *
 *	The @XH and @XJ types need special mention. XH and XJ are ascii formatting of addresses and integers respectively.
 *	BOTH are ASCII formatted hexdecimal output of a 64 bit sign-extended value.
 *	The present implementation of util_output does not support 'H'.
 *	This support was new in VMS 7.2 (and is one reason why GTM 4.2 requires VMS 7.2).
 *	The "@" designates an "indirect" request meaning that the address of
 *	the 8 byte item is passed rather than the item itself. This is what allows us to print 8 byte values in the
 *	non-Alpha 32 bit parameter worlds. These types are documented in the VMS System services manual under SYS$FAO.
 *	There are several other types that are supported on VMS but only these two were added on Unix.
 *
 *	In addition this implements another directive
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
	unsigned char	*numptr;
	boolean_t	right_justify, isprintable;

	VAR_COPY(last_va_list_ptr, fao);
	outptr = buff;
	outtop = outptr + size - 5;	/* 5 bytes to prevent writing across border 	*/
	/* 5 comes from line 268 -- 278			*/

	while (outptr < outtop)
	{
		/* Look for the '!' that starts an FAO directive */
		while ((schar = *message++) != '!')
		{
			if (schar == '\0')
			{
				va_end(last_va_list_ptr);	/* reset before using as dest in copy */
				VAR_COPY(last_va_list_ptr, fao);
				return outptr;
			}
			*outptr++ = schar;
			if (outptr >= outtop)
			{
				va_end(last_va_list_ptr);	/* reset before using as dest in copy */
				VAR_COPY(last_va_list_ptr, fao);
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
				}
				/* Since gtmsecshr does not load ICU libraries (since dlopen() with LD_LIBRARY_PATH
				 * does not work for root setuid executables), avoid calling gtm_wcswidth() and
				 * U_ISPRINT() from gtmsecshr and thus non-zero widths used in util_out_print()
				 * from gtmsecshr will not be treated as column widths but as character lengths.
				 * This is a safe limitation since no message from gtmsecshr specifies width yet.
				 */
				assert(!gtm_utf8_mode || IS_GTMSECSHR_IMAGE || (NULL != gtm_wcswidth_fnptr));
				cwidth = (!gtm_utf8_mode || IS_GTMSECSHR_IMAGE)
					? length : (*gtm_wcswidth_fnptr)((unsigned char *)c, length, FALSE, 1);
				if (0 < field_width && cwidth > field_width)
					cwidth = field_width;
				assert(0 <= cwidth); /* since all unprintable and illegal characters are ignored */
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
					assert('\0' != ch);	/* we dont expect <null> bytes in the middle of the string */
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
						   encounters a bad character, all platforms in utf8 mode
						   will behave as if !AF were specified and put a "." in place
						   of non-printable characters. SE 01/2007
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
						case 'S':		/* Signed value. Give sign if need to */
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
							} /* note fall into unsigned */
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
							{ /* length is number of ascii hex chars */
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
						if ('Z' != type && 'U' != type )
							GTMASSERT;
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
	va_end(last_va_list_ptr);	/* reset before using as dest in copy */
	VAR_COPY(last_va_list_ptr, fao);
	return outptr;
}

void	util_out_close(void)
{

	if ((NULL != util_outptr) && (util_outptr != util_outbuff))
		util_out_print("", FLUSH);
}

void	util_out_send_oper(char *addr, unsigned int len)
/* 1st arg: address of system log message */
/* 2nd arg: length of system long message (not used in Unix implementation) */
{
	sigset_t		savemask;

	if (first_syslog)
	{
		first_syslog = FALSE;
		(void)OPENLOG("GTM", LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_USER);
	}
	/*
	 * When syslog is processing and a signal occurs, the signal processing might eventually lead to another syslog
	 * call.  But in libc the first syslog has grabbed a lock (syslog_lock), and now the other syslog call will
	 * block waiting for that lock which can't be released since the first syslog was interrupted by the signal.
	 * A work around is to temporarily block signals (SIGINT, SIGQUIT, SIGTERM, SIGTSTP, SIGCONT, SIGALRM) and then
	 * restore them after the syslog call returns.
	 */
	/* It is possible for early process startup code to invoke this function so blocksig_initialized might not yet be set.
	 * An example C-stack is main/get_page_size/system-function interrupted by MUPIP STOP/generic_signal_handler/send_msg.
	 * Therefore this does not have an assert(blocksig_initialized) that similar code in other places (e.g. dollarh.c) has.
	 */
	if (blocksig_initialized)	/* In pro, dont take chances and handle case where it is not initialized */
		sigprocmask(SIG_BLOCK, &block_sigsent, &savemask);
	(void)SYSLOG(LOG_USER | LOG_INFO, "%s", addr);
	if (blocksig_initialized)
		sigprocmask(SIG_SETMASK, &savemask, NULL);
}


void	util_out_print_vaparm(caddr_t message, int flush, va_list var, int faocnt)
{
	char	fmt_buff[OUT_BUFF_SIZE];	/* needs to be same size as that of util_outbuff */
	caddr_t	fmtc;
	int	rc, count;
	char	*fmt_top1, *fmt_top2; /* the top of the buffer after leaving 1 (and 2 bytes respectively) at the end */
	int	util_avail_len;

	assert(SIZEOF(fmt_buff) == SIZEOF(util_outbuff));
	if (util_outptr == NULL)
		util_outptr = util_outbuff;
	if (message != NULL)
	{
		util_avail_len = INTCAST(util_outbuff + SIZEOF(util_outbuff) - util_outptr - 2);
		assert(0 <= util_avail_len);
		if (0 < util_avail_len)
			util_outptr = util_format(message, var, util_outptr, util_avail_len, faocnt);
	}
	switch (flush)
	{
		case NOFLUSH:
			break;
		case RESET:
			break;
		case FLUSH:
			*util_outptr++ = '\n';
		case OPER:
		case SPRINT:
			/* For all three of these actions we need to do some output buffer translation. In all cases a '%'
			 * is translated to the escape version '%%'. For OPER and SPRINT, we also translate '\n' to a ', '
			 * since some syslog() implementations (like Tru64) stop processing the passed message on a newline.
			 * Note that since the '%' -> '%%' or '\n' to ', ' translations imply an expansion in the buffer size
			 * requirements, we could potentially overflow the buffer after the translation. In that case we will
			 * stop copying just before the point of overflow is reached even though it means loss of the tail data.
			 */
			*util_outptr = '\0';
			fmt_top1 = fmt_buff + SIZEOF(fmt_buff) - 1;
			fmt_top2 = fmt_top1 - 1;
			for (util_outptr = util_outbuff, fmtc = fmt_buff; (0 != *util_outptr) && (fmtc < fmt_top1); )
			{
				if ('%' == *util_outptr)
				{
					if (fmtc >= fmt_top2) /* Check if there is room for 2 bytes. If not stop copying */
						break;
					*fmtc++ = '%';	/* escape for '%' */
					*fmtc++ = '%';
					util_outptr++;
				} else if ('\n' == *util_outptr && (OPER == flush || SPRINT == flush))
				{
					if (fmtc >= fmt_top2) /* Check if there is room for 2 bytes. If not stop copying */
						break;
					*fmtc++ = ',';
					*fmtc++ = ' ';
					util_outptr++;
				} else
					*fmtc++ = *util_outptr++;
			}
			assert(fmtc <= fmt_top1);
			*fmtc++ = '\0';
			switch (flush)
			{
				case FLUSH:
					FPRINTF(stderr, fmt_buff);
					break;
				case OPER:
					util_out_send_oper(fmt_buff, UINTCAST(fmtc - fmt_buff));
					break;
				case SPRINT:
					memcpy(util_outbuff, fmt_buff, fmtc - fmt_buff);
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
			/* Reset buffer information.  */
			util_outptr = util_outbuff;
			break;
	}
}

void	util_out_print(caddr_t message, int flush, ...)
{
	va_list	var;

	va_start(var, flush);

	util_out_print_vaparm(message, flush, var, MAXPOSINT4);
	va_end(last_va_list_ptr);
	va_end(var);
}

/* Used primarily by MUPIP in the MUPIP TRIGGER routines where output can either be output "normally" there or
 * when the same trigger parsing/loading functions are called from within GTM, the output is done with GTM IO
 * routines.
 */
void	util_out_print_gtmio(caddr_t message, int flush, ...)
{
	int		flush_it;
	boolean_t	usestdio;
	va_list		var;
	mval		flushtxt;

	va_start(var, flush);

	usestdio = IS_MCODE_RUNNING;
	assert((FLUSH == flush) || (NOFLUSH == flush));
	flush_it = ((FLUSH == flush) && !usestdio) ? FLUSH : NOFLUSH;
	util_out_print_vaparm(message, flush_it, var, MAXPOSINT4);
	if (usestdio && (FLUSH == flush))
	{	/* Message should be in buffer and we just need to flush it */
		assert(NULL != op_write_ptr);
		flushtxt.mvtype = MV_STR;
		flushtxt.str.addr = util_outbuff;
		flushtxt.str.len = INTCAST(util_outptr - util_outbuff);
		(*op_write_ptr)(&flushtxt);
		(*op_wteol_ptr)(1);
		util_outptr = util_outbuff;	/* Signal text is flushed */
	}
	va_end(last_va_list_ptr);
	va_end(var);
}

/* If $x of the standard output device is non-zero, and we are going to flush a buffer,
   put out a new line and then do the buffer flush. Called and used only by PRN_ERROR
   macro.
*/
void util_cond_flush(void)
{
	if (NULL != io_std_device.out && 0 < io_std_device.out->dollar.x && util_outptr != util_outbuff)
		FPRINTF(stderr, "\n");
	if (util_outptr != util_outbuff)
		util_out_print(NULL, FLUSH);
}

void util_out_save(void)
{
	if (NULL != util_outptr)
	{
		save_buff_used = MIN(OUT_BUFF_SIZE, ((NULL != util_outptr) ? (int4)(util_outptr - util_outbuff) : 0));
		if (0 != save_buff_used)
			memcpy(save_util_outbuff, util_outbuff, save_buff_used);
	}
}

void util_out_restore(void)
{
	if (0 != save_buff_used)
	{
		assert(OUT_BUFF_SIZE >= save_buff_used);
		memcpy(util_outbuff, save_util_outbuff, save_buff_used);
		util_outptr = util_outbuff + save_buff_used;
		save_buff_used = 0;
	}
}
