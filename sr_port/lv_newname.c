/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "mv_stent.h"
#include "lv_val.h"
#include "tp_frame.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "caller_id.h"
#include "alias.h"

GBLREF tp_frame		*tp_pointer;
GBLREF stack_frame	*frame_pointer;
GBLREF symval		*curr_symval;
GBLREF mv_stent		*mv_chain;

void lv_newname(ht_ent_mname *hte, symval *sym)
{
	lv_val		*lv, *var;
	tp_frame	*tf, *first_tf_saveall;
	tp_var		*restore_ent;
	DBGRFCT_ONLY(mident_fixed vname;)

	assert(hte);
	assert(sym);
	lv = lv_getslot(sym);
	LVVAL_INIT(lv, sym);
	DBGRFCT_ONLY(
		memcpy(vname.c, hte->key.var_name.addr, hte->key.var_name.len);
		vname.c[hte->key.var_name.len] = '\0';
	);
	DBGRFCT((stderr, "lv_newname: Varname '%s' in sym 0x"lvaddr" resetting hte 0x"lvaddr" from 0x"lvaddr" to 0x"lvaddr
		 " -- called from 0x"lvaddr"\n\n",
		 &vname.c, sym, hte, hte->value, lv, caller_id()));
	hte->value = lv;
	assert(0 < lv->stats.trefcnt);
	if (!sym->tp_save_all)
		return;
	/* Newly encountered variables need to be saved if there is restore all TP frame in effect as they
	   need to be restored to an undefined state but we only know about them when we encounter them
	   hence this code where new vars are created. We locate the earliest TP frame that has the same symval
	   in its tp_frame and save the entry there. This is so var set in later TP frame levels still get restored
	   even if the TSTART frame they were created in gets committed.
	*/
	DEBUG_ONLY(first_tf_saveall = NULL);
	for (tf = tp_pointer; (NULL != tf) && (tf->sym == sym); tf = tf->old_tp_frame)
	{
		if (tf->tp_save_all_flg)
			first_tf_saveall = tf;
	}
	assert(first_tf_saveall);
	assert(sym == LV_SYMVAL(lv));
	var = lv_getslot(sym);
	restore_ent = (tp_var *)malloc(SIZEOF(*restore_ent));
	restore_ent->current_value = lv;
	restore_ent->save_value = var;
	restore_ent->key = hte->key;
	restore_ent->var_cloned = TRUE;
	restore_ent->next = first_tf_saveall->vars;
	first_tf_saveall->vars = restore_ent;
	assert(NULL == lv->tp_var);
	lv->tp_var = restore_ent;
	*var = *lv;
	INCR_CREFCNT(lv);		/* With a copy made, bump the refcnt to keep lvval from being deleted */
	INCR_TREFCNT(lv);
	assert(1 < lv->stats.trefcnt);
}
