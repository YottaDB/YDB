/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "hashtab.h"
#include "lv_val.h"
#include "op.h"

GBLREF symval	*curr_symval;
GBLREF lv_val	*active_lv;
GBLREF uint4	lvtaskcycle;

void op_xkill(UNIX_ONLY_COMMA(int n) mval *lvname_arg, ...)
{
	va_list		var;
	VMS_ONLY(int	n;)
	DEBUG_ONLY(int	vcnt;)
	lv_val		*lv;
	mval		*lvname;
	mname_entry	lvent;
	ht_ent_mname	*tabent, *top;

	error_def(ERR_XKILLCNTEXC);

	active_lv = (lv_val *)NULL;	/* if we get here, subscript set was successful.  clear active_lv to avoid later
					   cleanup problems */
	VAR_START(var, lvname_arg);
	VMS_ONLY(va_count(n);)
	lvname = lvname_arg;
	assert(0 < n);
	/* In debug mode we want to make sure the elements we mark to not be deleted are the ONLY
	   elements that don't get deleted. The lvtaskcycle value, which is guaranteed to never be 0
	   (once it has been incremented via macro), is an excellent way to get a fairly unique way of
	   marking the hte. If we get positive marks that are NOT this values, we need to figure out why.
	*/
	DEBUG_ONLY(vcnt = 0);
	DEBUG_ONLY(INCR_LVTASKCYCLE);
	for (;  0 < n;)
	{
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
				assert(lv);
				assert(HTENT_VALID_MNAME(tabent, lv_val, lv));
				assert(MV_SYM == lv->ptrs.val_ent.parent.sym->ident);
				assert(!tabent->key.marked);
				tabent->key.marked = PRO_ONLY(TRUE) DEBUG_ONLY(lvtaskcycle);
				DEBUG_ONLY(vcnt++);
			}
		}
		if (0 < --n)
			lvname = va_arg(var, mval *);
		else
			break;
	};
	va_end(var);
	/* Perform "op_killall" processing except on those hash table entries we have marked */
	for (tabent = curr_symval->h_symtab.base, top = tabent + curr_symval->h_symtab.size; tabent < top; tabent++)
	{
		if (HTENT_VALID_MNAME(tabent, lv_val, lv))

			if (!tabent->key.marked)
				lv_kill(lv, TRUE);
			else
			{	/* If marked, unmark it and reduce debug count to verify array was as we expected it */
				assert(tabent->key.marked == lvtaskcycle);
				tabent->key.marked = FALSE;
				DEBUG_ONLY(vcnt--);
			}
	}
	assert(0 == vcnt);	/* Verify we found the same number as we marked */
}
