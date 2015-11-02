/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
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
#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#endif

GBLREF short int 	source_name_len, last_source_column, source_line;
GBLREF char 		source_file_name[];
GBLREF unsigned char	*source_buffer;
GBLREF bool		dec_nofac, run_time;

void show_source_line(char* buf, boolean_t warn)
{
	char 	*b, *c, *c_top;
	int	ch, chlen, chwidth;
	error_def(ERR_SRCLIN);
	error_def(ERR_SRCLOC);

	for (c = (char *)source_buffer, b = buf, c_top = c + last_source_column - 1; c < c_top; )
	{
		if (*c == '\t')
			*b++ = *c++;
		else if (!gtm_utf8_mode || *(uchar_ptr_t)c <= ASCII_MAX)
		{
			*b++ = ' ';
			c++;
		}
#ifdef UNICODE_SUPPORTED
		else
		{
			chlen = UTF8_MBTOWC(c, c_top, ch) - (uchar_ptr_t)c;
			if (WEOF != ch && 0 < (chwidth = UTF8_WCWIDTH(ch)))
			{
				memset(b, ' ', chwidth);
				b += chwidth;
			}
			c += chlen;
		}
#endif
	}
	memcpy(b, ARROW, STR_LIT_LEN(ARROW));
	b += STR_LIT_LEN(ARROW);
	*b = '\0';
	if (warn)
	{
		dec_nofac = TRUE;
		dec_err(VARLSTCNT (6) ERR_SRCLIN, 4, LEN_AND_STR((char *)source_buffer), b - buf, buf);
		if (!run_time)
			dec_err(VARLSTCNT(6) ERR_SRCLOC, 4, last_source_column, source_line, source_name_len, source_file_name);
		dec_nofac = FALSE;
	}
}
