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

#include "gtm_string.h"

#include "hashdef.h"
#include "lv_val.h"
#include "op.h"
#include <varargs.h>

typedef struct
{
	lv_val	lv;
	lv_val	*addr;
} save_lv;
#define MAX_SAVED_LV 256

GBLREF symval *curr_symval;
GBLREF lv_val *active_lv;
LITREF mident zero_ident;

void op_xkill(va_alist)
va_dcl
{
	va_list		var;
	int		n;
	save_lv		saved[MAX_SAVED_LV], *s, *s_top, *s_bot;
	ht_entry	*r;
	lv_val		*lv;
	mval		*lvname;
	mident		m;
	error_def(ERR_XKILLCNTEXC);

	active_lv = (lv_val *)0;	/* if we get here, subscript set was successful.  clear active_lv to avoid later
						cleanup problems */
	VAR_START(var);
	n = va_arg(var, int4);
	s = s_bot = &saved[0];
	s_top = s + MAX_SAVED_LV;
	for (;  n-- > 0;)
	{
		lvname = va_arg(var, mval *);
		if (lvname->str.len)
		{	/* convert mval to mident and see if it is in the symbol table */
			m = zero_ident;
			memcpy(&m.c[0], lvname->str.addr, (lvname->str.len < sizeof(mident)) ? lvname->str.len : sizeof(mident));
			if (r = ht_get(&curr_symval->h_symtab, (mname *)&m))
			{	/* save info about the variable */
				lv = (lv_val *)r->ptr;
				s->lv = *lv;
				s->addr = lv;
				lv->v.mvtype = 0;
				assert(lv->ptrs.val_ent.parent.sbs);
				lv->tp_var = NULL;
				lv->ptrs.val_ent.children = 0;
				s++;
				if (s >= s_top)
					rts_error(VARLSTCNT(3) ERR_XKILLCNTEXC, 1, MAX_SAVED_LV);
			}
		}
	}
	op_killall();

	/* restore the saved variables */
	for (s--;  s >= s_bot;  s--)
		*s->addr = s->lv;
}
