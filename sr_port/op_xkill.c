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

#include "gtm_string.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "hashtab.h"
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

void op_xkill(va_alist)
va_dcl
{
	va_list		var;
	int		n;
	save_lv		saved[MAX_SAVED_LV], *s, *s_top, *s_bot;
	lv_val		*lv;
	mval		*lvname;
	mname_entry	lvent;
	ht_ent_mname	*tabent;
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
		MV_FORCE_STR(lvname);
		if (lvname->str.len)
		{	/* convert mval to var_tabent and see if it is in the symbol table */
			if (lvname->str.len > MAX_MIDENT_LEN)
				lvname->str.len = MAX_MIDENT_LEN;
			lvent.var_name.len = lvname->str.len;
			lvent.var_name.addr = lvname->str.addr;
			COMPUTE_HASH_MNAME(&lvent);
			if (tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &lvent))
			{	/* save info about the variable */
				lv = (lv_val *)tabent->value;
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
