/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

#include "lv_val.h"
#include "op.h"

GBLREF symval		*curr_symval;
GBLREF lv_val		*active_lv;
GBLREF uint4		lvtaskcycle;
GBLREF boolean_t	gtm_stdxkill;

void op_xkill(UNIX_ONLY_COMMA(int n) mval *lvname_arg, ...)
{
	va_list		var;
	VMS_ONLY(int	n;)
	DEBUG_ONLY(int	vcnt;)
	lv_val		*lv;
	mval		*lvname;
	mname_entry	lvent;
	ht_ent_mname	*tabent, *top;
	boolean_t	lcl_stdxkill;

	active_lv = (lv_val *)NULL;	/* if we get here, subscript set was successful.  clear active_lv to avoid later
					   cleanup problems */
	/* GTM supports two methods for exclusive kill that affect the way aliases and pass-by-reference (PBR) parameters are
	 * treated when used in an exclusive kill.
	 *  - M Standard    - An alias or PBR var specified in an xkill list while its aliases are NOT in the list is killed.
	 *  - GTM alternate - In the same situation, the aliases are NOT killed.
	 *
	 * The differences in processing of these two xkill modes are small but distinct. We will loop through the var list in
	 * both cases but in the M Standard variant, we will mark the hash table entry as not to be killed in the lower kill loop
	 * while in the GTM variant, we will mark the lv_val instead.
	 */
	lcl_stdxkill = gtm_stdxkill;
	VAR_START(var, lvname_arg);
	VMS_ONLY(va_count(n);)
	lvname = lvname_arg;
	assert(0 < n);
	/* In debug mode we want to make sure the elements we mark to not be deleted are the ONLY
	 * elements that don't get deleted. The lvtaskcycle value, which is guaranteed to never be 0
	 * (once it has been incremented via macro), is an excellent way to get a fairly unique way of
	 * marking the hte. If we get positive marks that are NOT this value, we need to figure out why.
	 */
#	ifdef DEBUG
	vcnt = 0;
	INCR_LVTASKCYCLE;		/* Needed for both debug of MSTD version or to mark lv_vals */
#	else
	if (!lcl_stdxkill)
	{
		INCR_LVTASKCYCLE;	/* Needed to mark lv_vals */
	}
#	endif
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
				assert(LV_IS_BASE_VAR(lv));
				assert(HTENT_VALID_MNAME(tabent, lv_val, lv));
				if (lcl_stdxkill)
				{	/* M Standard xkill variant - mark the hash table entry so it is not deleted */
					assert(!tabent->key.marked);
					tabent->key.marked = PRO_ONLY(TRUE) DEBUG_ONLY(lvtaskcycle);
					DEBUG_ONLY(vcnt++);
				} else
				{	/* GTM xkill variant - mark the lvval so it is not deleted for ANY variant */
					DEBUG_ONLY(if (lv->stats.lvtaskcycle != lvtaskcycle) vcnt++;);
					lv->stats.lvtaskcycle = lvtaskcycle;
				}
			}
		}
		if (0 < --n)
			lvname = va_arg(var, mval *);
		else
			break;
	}
	va_end(var);
	/* Perform "op_killall" processing except on those hash table entries we have marked */
	for (tabent = curr_symval->h_symtab.base, top = tabent + curr_symval->h_symtab.size; tabent < top; tabent++)
	{
		if (HTENT_VALID_MNAME(tabent, lv_val, lv))
		{
			assert(LV_IS_BASE_VAR(lv));
			if (lcl_stdxkill)
			{	/* M Standard variant - delete this var if hashtab entry not marked (delete by name) */
				if (!tabent->key.marked)
					lv_kill(lv, DOTPSAVE_TRUE, DO_SUBTREE_TRUE);
				else
				{	/* If marked, unmark it and reduce debug count to verify array was as we expected it */
					assert(tabent->key.marked == lvtaskcycle);
					tabent->key.marked = FALSE;
					DEBUG_ONLY(vcnt--);
				}
			} else
			{	/* GTM variant - delete var if lv_val is not marked (delete by value) */
				if (lv->stats.lvtaskcycle != lvtaskcycle)
					lv_kill(lv, DOTPSAVE_TRUE, DO_SUBTREE_TRUE);
				/* Note there is no (easy) way to maintain the debug vcnt in this variant */
			}
		}
	}
	assert(!lcl_stdxkill || (0 == vcnt));	/* Verify we found the same number as we marked but only for M-Standard variant */
}
