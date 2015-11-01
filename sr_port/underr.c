/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "underr.h"
#include <varargs.h>
#include "hashtab_mname.h"    /* needed for lv_val.h */
#include "lv_val.h"
#include "undx.h"

GBLDEF bool	undef_inhibit = 0;
LITREF mval	literal_null;

void	underr (va_alist)
va_dcl
{
	mval		*start;
	mident_fixed	name;
	unsigned char	*end;
	va_list		var;		/* this is a dummy so we can pass a va_list to undx */
	error_def(ERR_UNDEF);

	va_start (var);
	start = va_arg(var, mval *);
	if (start && undef_inhibit)
		*start = literal_null;
	else
	{
		end = format_lvname((lv_val *)start, (uchar_ptr_t)name.c, sizeof(name));
		rts_error(VARLSTCNT(4) ERR_UNDEF, 2, ((char *)end - name.c), name.c);
	}
	return;
}
