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
#include "hashdef.h"
#include "lv_val.h"
#include "op.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include <varargs.h>

GBLREF symval		*curr_symval;
GBLREF unsigned char	*stackbase, *msp;
LITREF mident		zero_ident;

void op_xnew(va_alist) /* SHOULD BE MODIFIED TO USE AN EXISTING POINTER TO SYMTAB */
va_dcl
{
	va_list var;
	unsigned int argcnt;
	mval *s;
	mident	m;
	int4 shift;
	ht_entry *q,*r;
	bool new;

	VAR_START(var);
	argcnt = va_arg(var, int4);
	shift = symbinit();
	for (; argcnt-- > 0 ; )
	{
		m = zero_ident;
		s = va_arg(var, mval*);
		if ((unsigned char *)s >= msp && (unsigned char *)s < stackbase)
			s = (mval*)((char*)s - shift);		/* Only if stack resident */
		memcpy(&m.c[0], s->str.addr, (s->str.len < sizeof(mident)) ? s->str.len : sizeof(mident));
		assert(curr_symval->last_tab);

		q = ht_put(&(curr_symval->last_tab->h_symtab),(mname *)&m, &new);
		if (new)
			lv_newname(q, curr_symval->last_tab);
		r = ht_put(&curr_symval->h_symtab, (mname *)&m, &new);
		r->ptr = q->ptr;
	}
	return;
}
