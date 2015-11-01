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

#include "stringpool.h"
#include "gtm_iconv.h"
#include "io.h"
#include "iosp.h"
#ifdef __MVS__
#include "gtm_unistd.h"
#endif
#include "op.h"
#include <stdarg.h>
#include "ebc_xlat.h"

GBLREF spdesc stringpool;

void op_fnchar(UNIX_ONLY_COMMA(int cnt) mval *dst, ...)
{
	va_list var;
	int ch;
	int ebcdic_trans;
	unsigned char *base;
	VMS_ONLY(int	cnt;)
	error_def(ERR_TEXT);
	iconv_t	tmp_cvt_cd;
	unsigned char	*tmp_ptr;
	unsigned int	tmp_len;

	VAR_START(var, dst);
	VMS_ONLY(va_count(cnt);)
	cnt -= 2;

	if (stringpool.free + cnt > stringpool.top)
		stp_gcol(cnt);

	ebcdic_trans = va_arg(var, int4);
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
	dst->str.len = (char *)base - dst->str.addr;
	stringpool.free += dst->str.len;
	if (ebcdic_trans)
	{
		*base = '\0';
		tmp_ptr = (unsigned char *)dst->str.addr;
		tmp_len = dst->str.len;
		ICONV_OPEN_CD(tmp_cvt_cd, "IBM-1047", "ISO8859-1");
		ICONVERT(tmp_cvt_cd, &tmp_ptr, &tmp_len, &tmp_ptr, &tmp_len);
		ICONV_CLOSE_CD(tmp_cvt_cd);
	}
}
