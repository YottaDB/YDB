/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"
#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF stack_frame	*frame_pointer;
GBLREF symval		*curr_symval;
GBLREF uint4		dollar_tlevel;
GBLREF lv_val		*active_lv;

/* Operation - Kill an alias (unsubscripted variable)
 *
 * Look it up in the hash table and remove the pointer to the lv_val to destroy the
 * alias association. Will need to do this in any previous symtabs this var is in
 * as well.
 */
void op_killalias(int srcindx)
{
	ht_ent_mname	*tabent;
	mname_entry	*varname;
	lv_val		*lv;
	int4		symvlvl;

	active_lv = (lv_val *)NULL;	/* if we get here, subscript set was successful.  clear active_lv to avoid later
					 * cleanup problems.
					 */
	varname = &(((mname_entry *)frame_pointer->vartab_ptr)[srcindx]);
	tabent = lookup_hashtab_mname(&curr_symval->h_symtab, varname);		/* Retrieve hash tab entry this var */
	if (tabent)
	{
		lv = (lv_val *)tabent->value;
		assert(lv);
		assert(LV_IS_BASE_VAR(lv));
		symvlvl = (LV_GET_SYMVAL(lv))->symvlvl;
		/* Clone var if necessary */
		if (dollar_tlevel && (NULL != lv->tp_var) && !lv->tp_var->var_cloned)
			TP_VAR_CLONE(lv);
		/* Decrement reference count and cleanup if necessary */
		DECR_BASE_REF(tabent, lv, TRUE);
		MARK_ALIAS_ACTIVE(symvlvl);	/* Mark this entry as aliasly active now */
	} /* Else var has no hastable entry so this is a NOOP */
}
