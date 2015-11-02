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
#include "error.h"
#include "op.h"
#include "patcode.h"
#include "mvalconv.h"
#ifdef	UNICODE_SUPPORTED
#include <wchar.h>
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

GBLREF	boolean_t	badchar_inhibit;
GBLREF	boolean_t	gtm_utf8_mode;
GBLREF	uint4		*pattern_typemask;

/* Routine to compute the display width of a string */
void op_fnzwidth(mval* src, mval* dst)
{
	unsigned char	*srctop, *srcptr, *nextptr;
	int 		width;

	MV_FORCE_STR(src);

	srcptr = (unsigned char *)src->str.addr;
#ifdef	UNICODE_SUPPORTED
	if (!gtm_utf8_mode)
	{
#endif
		width = src->str.len;
		for (srctop = srcptr + src->str.len; srcptr < srctop; ++srcptr)
		{ /* All non-control characters are printable. Control characters are ignored (=0 width) in width calculations. */
			if ((pattern_typemask[*srcptr] & PATM_C))
				width -= 1;
		}
#ifdef	UNICODE_SUPPORTED
	} else
		width = gtm_wcswidth(srcptr, src->str.len, TRUE, 0);	/* TRUE => strict checking of BADCHARs */
#endif
	MV_FORCE_MVAL(dst, width);
}
