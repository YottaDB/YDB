/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "compiler.h"
#include "show_source_line.h"
#include "gtmmsg.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#endif

GBLREF short int 	source_name_len, source_line;
GBLREF char 		source_file_name[];
GBLREF unsigned char	*source_buffer;
GBLREF bool		dec_nofac;
GBLREF boolean_t	run_time;

error_def(ERR_SRCLIN);
error_def(ERR_SRCLOC);
error_def(ERR_SRCLNNTDSP);
error_def(ERR_ARROWNTDSP);

#define MAXLINESIZEFORDISPLAY 1023

void show_source_line(boolean_t warn)
{
	char 		*b, *b_top, *c, *c_top, *buf;
	char		source_line_buff[MAX_SRCLINE + SIZEOF(ARROW)];
	ssize_t		buflen;
	int		chlen, chwidth;
	unsigned int	ch, line_chwidth = 0;
	boolean_t	unable_to_complete_arrow = FALSE;
	mstr		msgstr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	buf = source_line_buff;
	buflen = SIZEOF(source_line_buff);
	b_top = buf + buflen - STR_LIT_LEN(ARROW) - 1; /* allow room for arrow and string terminator */
	for (c = (char *)source_buffer, b = buf, c_top = c + TREF(last_source_column) - 1; c < c_top;)
	{
		if ('\t' == *c)
		{
			if ((b + 1) > b_top)
			{
				unable_to_complete_arrow = TRUE;
				break;
			}
			*b++ = *c++;
		}
		else if (!gtm_utf8_mode || (ASCII_MAX >= *(uchar_ptr_t)c))
		{
			if ((b + 1) > b_top)
			{
				unable_to_complete_arrow = TRUE;
				break;
			}
			*b++ = ' ';
			c++;
		}
#		ifdef UNICODE_SUPPORTED
		else
		{
			chlen = (int)(UTF8_MBTOWC(c, c_top, ch) - (uchar_ptr_t)c);
			if (WEOF != ch && (0 < (chwidth = UTF8_WCWIDTH(ch))))	/* assignment */
			{
				if ((b + chwidth) > b_top)
				{
					unable_to_complete_arrow = TRUE;
					break;
				}
				memset(b, ' ', chwidth);
				b += chwidth;
			}
			c += chlen;
		}
#		endif
	}
	if (unable_to_complete_arrow)
	{
		msgstr.addr = buf;
		msgstr.len = buflen;
		dec_nofac = TRUE;
		gtm_getmsg(ERR_ARROWNTDSP, &msgstr);
		dec_nofac = FALSE;
	} else
	{
		memcpy(b, ARROW, STR_LIT_LEN(ARROW));
		b += STR_LIT_LEN(ARROW);
		*b = '\0';
	}
	if (warn)
	{
		for (c = (char *)source_buffer; c < (char *)source_buffer + STRLEN((char *)source_buffer) - 1; )
		{
			if ('\t' == *c)
			{
				line_chwidth++;
				c++;
			}
			else if (!gtm_utf8_mode || (ASCII_MAX >= *(uchar_ptr_t)c))
			{
				line_chwidth++;
				c++;
			} else
			{
#			ifdef UNICODE_SUPPORTED		/* funky positioning makes VMS compiler happy */
				chlen = (int)(UTF8_MBTOWC(c, (char *)source_buffer + STRLEN((char *)source_buffer) - 1, ch)
						- (uchar_ptr_t)c);
				if ((WEOF != ch) && 0 < (chwidth = UTF8_WCWIDTH(ch)))
					line_chwidth += chwidth;
				c += chlen;
#			endif
			}
		}
		dec_nofac = TRUE;
		if (MAXLINESIZEFORDISPLAY > line_chwidth)
			if (unable_to_complete_arrow)
				dec_err(VARLSTCNT(6) ERR_SRCLIN, 4, LEN_AND_STR((char *)source_buffer), msgstr.len, msgstr.addr);
			else
				dec_err(VARLSTCNT(6) ERR_SRCLIN, 4, LEN_AND_STR((char *)source_buffer), b - buf, buf);
		else
			dec_err(VARLSTCNT(2) ERR_SRCLNNTDSP, 1, MAXLINESIZEFORDISPLAY);
		if (!run_time)
			dec_err(VARLSTCNT(6) ERR_SRCLOC, 4, TREF(last_source_column), source_line,
				source_name_len, source_file_name);
		dec_nofac = FALSE;
	}
}
