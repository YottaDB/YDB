/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "stringpool.h"
#include "gtm_iconv.h"
#include "io.h"
#include "iosp.h"
#ifdef __MVS__
#include "gtm_unistd.h"
#endif
#include "op.h"
#include <stdarg.h>

GBLREF spdesc stringpool;

#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
GBLREF	boolean_t	badchar_inhibit;
error_def(ERR_INVDLRCVAL);

/* Multi-byte implementation of $CHAR() that creates a string from Unicode codes */
void op_fnchar(UNIX_ONLY_COMMA(int cnt) mval *dst, ...)
{
	va_list 	var;
	int4 		ch, size, char_len;
	unsigned char 	*base, *outptr, *tmpptr;
	VMS_ONLY(int	cnt;)

	VAR_START(var, dst);
	VMS_ONLY(va_count(cnt);)
	cnt -= 1;

	size = cnt * GTM_MB_LEN_MAX;
	ENSURE_STP_FREE_SPACE(size);
	base = stringpool.free;
	for (outptr = base, char_len = 0; cnt > 0; --cnt)
	{
		ch = va_arg(var, int4);
		if (ch >= 0)
		{ /* As per the M standard, negative code points should map to no characters */
			tmpptr = UTF8_WCTOMB(ch, outptr);
			assert(tmpptr - outptr <= 4);
			if (tmpptr != outptr)
				++char_len; /* yet another valid character. update the character length */
			else if (!badchar_inhibit)
				rts_error(VARLSTCNT(3) ERR_INVDLRCVAL, 1, ch);
			outptr = tmpptr;
		}
	}
	va_end(var);
	MV_INIT_STRING(dst, outptr - base, base);
	dst->str.char_len = char_len;
	dst->mvtype |= MV_UTF_LEN;
	stringpool.free += dst->str.len;
}
#endif /* UNICODE_SUPPORTED */

/* Single-byte implementation of $CHAR() that creates a string from ASCII codes */
void op_fnzchar(UNIX_ONLY_COMMA(int cnt) mval *dst, ...)
{
	va_list 	var;
	int 		ch;
	unsigned char 	*base;
	VMS_ONLY(int	cnt;)

	VAR_START(var, dst);
	VMS_ONLY(va_count(cnt);)
	cnt -= 1;

	ENSURE_STP_FREE_SPACE(cnt);

	dst->mvtype = MV_STR;
	dst->str.addr = (char *)stringpool.free;
	base = stringpool.free;

	while (cnt-- > 0)
	{
		ch = va_arg(var, int4);
		if ((ch >= 0) && (ch < 256))	/* only true for single byte character set */
			*base++ = ch;
	}
	va_end(var);
	dst->str.len = INTCAST((char *)base - dst->str.addr);
	stringpool.free += dst->str.len;
}
