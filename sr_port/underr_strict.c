/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>

#include "lv_val.h"
#include "undx.h"

error_def(ERR_UNDEF);

mval *underr_strict(mval *start, ...)
{
	mident_fixed    name;
	unsigned char   *end;
	va_list         var;

	/* Variation of underr in which we ignore NOUNDEF */
	va_start (var, start);
	end = format_lvname((lv_val *)start, (uchar_ptr_t)name.c, SIZEOF(name));
	va_end(var);
	rts_error(VARLSTCNT(4) ERR_UNDEF, 2, ((char *)end - name.c), name.c);
	return (mval *)NULL; /* To keep compiler happy */
}
