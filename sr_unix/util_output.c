/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

/* LinuxIA32/gcc needs stdio before varargs due to stdarg */
/* Linux390/gcc needs varargs before stdarg */
#ifdef EARLY_VARARGS
#include <varargs.h>
#endif
#include "gtm_stdio.h"
#ifndef EARLY_VARARGS
#include <varargs.h>
#endif
#include "gtm_syslog.h"
#include <errno.h>

#include "io.h"
#include "error.h"
#include "fao_parm.h"
#include "min_max.h"
#include "util.h"
#include "util_format.h"
#include "util_out_print_vaparm.h"

#define GETFAOVALDEF(faocnt, var, type, result, defval) \
	if (faocnt > 0) {result = (type)va_arg(var, type); faocnt--;} else result = defval;

GBLREF	io_pair		io_std_device;
GBLDEF	char		*util_outptr, util_outbuff[OUT_BUFF_SIZE];
GBLDEF	va_list		last_va_list_ptr;
static	boolean_t	first_syslog = TRUE;

/*
 *	This routine implements a SUBSET of FAO directives, namely:
 *
 *		!/	!_	!^	!!
 *
 *		!mAC	!mAD	!mAF	!mAS	!mAZ
 *
 *		!mSB	!mSW	!mSL
 *
 *		!mUB	!mUW	!mUL
 *
 *		!mXB	!mXW	!mXL
 *
 *		!mZB	!mZW	!mZL
 *
 *		!n*c
 *
 *	Where `m' is an optional field width, `n' is a repeat count,
 *	and `c' is a single character.  `m' or `n' may be specified
 *	as the '#' character, in which case the value is taken from
 *	the next parameter.
 *
 *	FAO stands for "formatted ASCII output".  The FAO directives
 *	may be considered equivalent to format specifications and are
 *	documented with the VMS Lexical Fuction F$FAO in the OpenVMS
 *	DCL Dictionary.
 */

/*
 *	util_format - convert FAO format string to C printf format string.
 *
 *	input arguments:
 *		message	- one of the message strings from, for example, merrors.c
 *		fao	- list of values to be inserted into message according to
 *			  the FAO directives
 *		size	- size of buff
 *
 *	output argument:
 *		buff	- will contain C printf-style format statement with any
 *			  "A" (character) fields filled in from fao list
 *
 *	output global value:
 *		outparm[] - array of numeric arguments from fao list (character
 *			    arguments already incorporated into buff
 */

caddr_t util_format(caddr_t message, va_list fao, caddr_t buff, int4 size, int faocnt)
{
	desc_struct	*d;
	signed char	schar;
	unsigned char	type, type2;
	caddr_t		c, outptr, outtop;
	unsigned char	uchar;
	short		sshort, *s;
	unsigned short	ushort;
	int		i, length, max_chars, field_width, repeat_count, int_val;
	boolean_t	indirect;
	qw_num_ptr_t	val_ptr;
	unsigned char	numa[22];
	unsigned char	*numptr;

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
				VAR_COPY(last_va_list_ptr, fao);
				return outptr;
			}
			*outptr++ = schar;
			if (outptr >= outtop)
			{
				VAR_COPY(last_va_list_ptr, fao);
				return outptr;
			}
		}

		field_width = 0;	/* Default values */
		repeat_count = 1;
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

			if ((length = c - message) > 0)
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
				while ((repeat_count-- > 0) && (outptr < outtop))
					*outptr++ = *message;
				++message;
				continue;

			case 'A':
				assert(!indirect);
				switch(type2 = *message++)
				{
					case 'C':
						GETFAOVALDEF(faocnt, fao, caddr_t, c, NULL);
						length = c ? *c++ : 0;
						break;

					case 'D':
					case 'F':
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

					case 'Z':
						GETFAOVALDEF(faocnt, fao, caddr_t, c, NULL);
						length = c ? strlen(c) : 0;
				}

				max_chars = MIN((outtop - outptr - 1),
						(0 == field_width ? length : MIN(field_width, length)));

				for (i = 0;  i < max_chars;  ++i, ++c)
					if (type2 == 'F'  &&  (*c < ' '  ||  *c > '~'))
						*outptr++ = '.';
					else if (*c == '\0')
						--i;	/* Don't count nul characters */
					else
						*outptr++ = *c;

				assert(0 <= field_width);
				assert(0 <= max_chars);
				for (i = field_width - max_chars;  i > 0;  --i)
					*outptr++ = ' ';
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
							default:
								assert(FALSE);
						}
					else
					{
						GETFAOVALDEF(faocnt, fao, int4, int_val, 0);
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
							default:
								assert(FALSE);
						}
					}
					switch (type)
					{
						case 'S':		/* Signed value. Give sign if need to */
							if (0 > int_val)
							{
								*numptr++ = '-';
								int_val = -(int_val);
							}		/* note fall into unsigned */
						case 'U':
						case 'Z':		/* zero filled */
							numptr = i2asc(numptr, int_val);
							break;
						case 'X':		/* Hex */
							switch (type2)
							{ /* length is number of ascii hex chars */
								case 'B':
							        	length = sizeof(short);
							         	break;
								case 'W':
									length = sizeof(int4);
							                break;
							        case 'L':
						               		length = sizeof(int4) + sizeof(int4);
						                       	break;
								default:
									assert(FALSE);
							}
							i2hex(int_val, numptr, length);
							numptr += length;
							break;
						default:
							assert(FALSE);
					}
				} else
				{
					if ('X' != type)			/* Only support XH and XJ */
						GTMASSERT;
					assert('H' == type2 || 'J' == type2);
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
				}
				length = numptr - numa;		/* Length of asciified number */
				max_chars = MIN((outtop - outptr - 1),
						(0 == field_width ? length : MIN(field_width, length)));
				if (length < field_width)
				{
					memset(outptr, (('Z' == type) ? '0' : ' '), field_width - length);
					outptr += field_width - length;
				}
				if ((field_width > 0) && (field_width < length))
				{
					memset(outptr, '*', field_width);
					outptr += field_width;
				} else
				{
					memcpy(outptr, numa, length);
					outptr += length;
				}
		}
	}
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
	if (first_syslog)
	{
		first_syslog = FALSE;
		(void)OPENLOG("GTM", LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_USER);
	}
	(void)SYSLOG(LOG_USER | LOG_INFO, addr);
}


void	util_out_print_vaparm(caddr_t message, int flush, va_list var, int faocnt)
{
	char	fmt_buff[OUT_BUFF_SIZE];
	caddr_t	fmtc;
	int	rc;

	/*
	 * Note: this function checks for EINTR on FPRINTF. This check should not be
	 * converted to an EINTR wrapper macro because of the variable number of args used
	 * by fprintf.
	 */

	if (util_outptr == NULL)
		util_outptr = util_outbuff;

	if (message != NULL)
		util_outptr = util_format(message, var, util_outptr, OUT_BUFF_SIZE - (util_outptr - util_outbuff) - 2, faocnt);

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
			/******************************************************************************************************
			   For all three of these actions we need to do some output buffer translation. In all cases a '%'
			   is translated to the escape version '%%'. For OPER and SPRINT, we also translate '\n' to a ', '
			   since some syslog() implementations (like Tru64) stop processing the passed message on a newline.
			*******************************************************************************************************/
			*util_outptr = '\0';
			for (util_outptr = util_outbuff, fmtc = fmt_buff;  0 != *util_outptr; )
			{
				if ('%' == *util_outptr)
				{
					*fmtc++ = '%';	/* escape for '%' */
					*fmtc++ = '%';
					util_outptr++;
				} else if ('\n' == *util_outptr && (OPER == flush || SPRINT == flush))
				{
					*fmtc++ = ',';
					*fmtc++ = ' ';
					util_outptr++;
				} else
					*fmtc++ = *util_outptr++;
			}
			*fmtc++ = '\0';
			switch (flush)
			{
				case FLUSH:
					do
					{
						rc = FPRINTF(stderr, fmt_buff);
					} while (-1 == rc && EINTR == errno);
					break;
				case OPER:
					util_out_send_oper(STR_AND_LEN(fmt_buff));
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

void	util_out_print(va_alist)
va_dcl
{
	va_list	var;
	caddr_t	message;
	int	flush;

	va_start(var);
	message = va_arg(var, caddr_t);
	flush = va_arg(var, int);

	util_out_print_vaparm(message, flush, var, MAXPOSINT4);
}

/* If $x of the standard output device is non-zero, put out a new line. Specifically
   this is used in the PRN_ERROR macro to put a newline char prior to flushing a buffer
   with an error message in it. */
void util_cond_newline(void)
{
	if (NULL != io_std_device.out && 0 < io_std_device.out->dollar.x && util_outptr != util_outbuff)
		FPRINTF(stderr, "\n");
}
