/****************************************************************
 *                                                              *
 * Copyright (c) 2009-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "gtmio.h"
#include "lv_val.h"
#include "op.h"
#include "gdsroot.h"
#include "stringpool.h"
#include "stp_parms.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"
#include "stack_frame.h"

GBLREF symval           *curr_symval;
GBLREF uint4		dollar_tlevel;

/* Delete all aliases and the data they point to.
 *
 * Two things need to happen:
 * 1) Scan the hash table for aliased base variables and remove them.
 * 2) Scan ALL subscripted vars for containers, delete the data they point to maintaining
 *    proper maintenance of reference counts and unmark the container making it a normal value.
 *
 * Since an alias with two references ceases being an alias if a reference is killed, we
 * cannot just do a simple scan and delete references. We will leave "klingons" who used to
 * be aliases but now are not. So we do this in a 3 step procedure:
 * 1) Identify all overt aliases in the hash table.
 * 2) For the entries that are not aliases but which contain alias containers, scan those
 *    arrays for containers, remove the container attribute and do the necessary refcnt
 *    cleanup. When all containers have been removed (including step 3), any remaining
 *    orphaned data will be recovered by the next LVGC.
 * 3) Go through the list of HTEs recorded in step one and kill their hash table reference.
 */
void op_killaliasall(void)
{
	ht_ent_mname    *tabent, *tabent_top;
	lv_val		*lvp, *lvp_top, *lvrefp;
	symval		*symv;
	int		lowest_symvlvl;
	ht_ent_mname	**htearraycur = NULL, **htearray = NULL, **htearraytop = NULL;

	SET_ACTIVE_LV(NULL, TRUE, actlv_op_killaliasall);	/* If we get here, subscript set was successful.
								 * Clear active_lv to avoid later cleanup issues */
	lowest_symvlvl = MAXPOSINT4;

	/* First pass through hash table we record HTEs that have > 1 trefcnt. We will delete these in a later
	 * loop but don't want to delete any until all are found.
	 */
	for (tabent = curr_symval->h_symtab.base, tabent_top = curr_symval->h_symtab.top; tabent < tabent_top; tabent++)
	{
		if (HTENT_VALID_MNAME(tabent, lv_val, lvp) && lvp && (1 < lvp->stats.trefcnt))
		{	/* Verify room in the table, expand if necessary */
			assert(LV_IS_BASE_VAR(lvp));
			ADD_TO_STPARRAY(tabent, htearray, htearraycur, htearraytop, ht_ent_mname);
			/* Need to find the lowest level symval that is affected by this kill * so we can mark all necessary
			 * symvals as having had alias activity.
			 */
			lvp = (lv_val *)tabent->value;
			symv = LV_GET_SYMVAL(lvp);
			assert(0 <= symv->symvlvl);
			if (lowest_symvlvl > symv->symvlvl)
				lowest_symvlvl = symv->symvlvl;
		}
	}
	/* This next, less scenic trip through the hash table entries we scan any arrays we
	 * find for containers that must be dealt with. We couldn't deal with these until all
	 * the "blatant" aliases were identified.
	 */
	for (tabent = curr_symval->h_symtab.base, tabent_top = curr_symval->h_symtab.top; tabent < tabent_top; tabent++)
	{
		if (HTENT_VALID_MNAME(tabent, lv_val, lvp) && lvp && (1 == lvp->stats.trefcnt))
		{	/* Var was not an alias but now need to check if var has any containers in it
			 * that likewise need to be processed (and de-container-ized).
			 */
			assert(LV_IS_BASE_VAR(lvp));
			KILL_CNTNRS_IN_TREE(lvp);			/* Note macro has LV_GET_CHILD() check in it */
		}
	}
	/* Now we can go through the hash table entries we identified in the first step and delete them.  */
	for (htearraytop = htearraycur, htearraycur = htearray; htearraycur < htearraytop; ++htearraycur)
	{
		assert(htearraycur);
		tabent = *htearraycur;
		lvp = (lv_val *)tabent->value;
		assert(lvp);
		assert(LV_IS_BASE_VAR(lvp));
		assert(0 < lvp->stats.trefcnt);
		/* Clone var if necessary */
		if (dollar_tlevel && (NULL != lvp->tp_var) && !lvp->tp_var->var_cloned)
			TP_VAR_CLONE(lvp);
		/* Decrement reference count and cleanup if necessary */
		DECR_BASE_REF(tabent, lvp, TRUE);
	}
	/* Now mark all symvals from the earliest affected by our command to the current as having had alias activity */
	MARK_ALIAS_ACTIVE(lowest_symvlvl);
  }
