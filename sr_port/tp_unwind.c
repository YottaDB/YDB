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

#include <signal.h>             /* for VSIG_ATOMIC_T type */
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "rtnhdr.h"
#include "mv_stent.h"
#include "stack_frame.h"
#include "tp_frame.h"
#include "mlkdef.h"
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "hashtab.h"
#include "lv_val.h"
#include "op.h"
#include "tp_unwind.h"
#include "mlk_pvtblk_delete.h"	/* for prototype */
#include "mlk_unlock.h"		/* for prototype */
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"
#include "have_crit.h"
#ifdef UNIX
#include "deferred_signal_handler.h"
#endif

GBLREF	stack_frame	*frame_pointer;
GBLREF	unsigned char	*msp, *stackbase, *stacktop;
GBLREF	mv_stent	*mv_chain;
GBLREF	tp_frame	*tp_pointer;
GBLREF	unsigned char	*tp_sp, *tpstackbase, *tpstacktop;
GBLREF	mlk_pvtblk	*mlk_pvt_root;
GBLREF	short		dollar_tlevel;
GBLREF	symval		*curr_symval;

void	tp_unwind(short newlevel, enum tp_unwind_invocation invocation_type)
{
	mlk_pvtblk	**prior, *mlkp;
	mlk_tp		*oldlock, *nextlock;
	int		tl;
	lv_val		*save_lv, *curr_lv, *lv;
	tp_var		*restore_ent;
	mv_stent	*mvc;
	boolean_t	restore_lv, rollback_locks;
	lvscan_blk	*lvscan, *lvscan_next, first_lvscan;
	int		elemindx;
	int		save_intrpt_ok_state;

	error_def(ERR_STACKUNDERFLO);

	/* We are about to clean up structures. Defer MUPIP STOP/signal handling until function end. */
	SAVE_INTRPT_OK_STATE(INTRPT_IN_TP_UNWIND);

	DBGRFCT((stderr, "\ntp_unwind: Beginning TP unwind process\n"));
	restore_lv = (invocation_type == RESTART_INVOCATION);
	lvscan = &first_lvscan;
	lvscan->next = NULL;
	lvscan->elemcnt = 0;
	assert(tp_sp <= tpstackbase  &&  tp_sp > tpstacktop);
	assert(tp_pointer <= (tp_frame *)tpstackbase  &&  tp_pointer > (tp_frame *)tpstacktop);
	for (tl = dollar_tlevel;  tl > newlevel;  --tl)
	{
		DBGRFCT((stderr, "\ntp_unwind: Unwinding level %d -- tp_pointer: 0x"lvaddr"\n", tl, tp_pointer));
		assert(NULL != tp_pointer);
		for (restore_ent = tp_pointer->vars;  NULL != restore_ent;  restore_ent = tp_pointer->vars)
		{
			/*********************************************************************************/
			/* TP_VAR_CLONE sets the var_cloned flag, showing that the tree has been cloned  */
			/* If var_cloned is not set, it shows that curr_lv and save_lv are still sharing */
			/* the tree, so it should not be killed.                                         */
			/*********************************************************************************/
			curr_lv = restore_ent->current_value;
			save_lv = restore_ent->save_value;
			assert(curr_lv);
			assert(save_lv);
			assert(MV_SYM == curr_lv->ptrs.val_ent.parent.sym->ident);
			assert(MV_SYM == save_lv->ptrs.val_ent.parent.sym->ident);
			assert(0 < curr_lv->stats.trefcnt);
			assert(curr_lv->tp_var);
			assert(curr_lv->tp_var == restore_ent);

			/* In order to restart sub-transactions, this would have to maintain
			   the chain that currently is not built by op_tstart() */
			if (restore_lv)
				tp_unwind_restlv(curr_lv, save_lv, restore_ent, NULL);
			else if (NULL != save_lv->ptrs.val_ent.children)
			{
				DBGRFCT((stderr, "\ntp_unwind: Not restoring curr_lv and has children\n"));
				if (curr_lv->tp_var->var_cloned)
				{	/* If cloned, we have to blow away the cloned tree */
					assert(save_lv->tp_var);
					DBGRFCT((stderr,"\ntp_unwind: For lv_val 0x"lvaddr": Deleting saved lv_val 0x"lvaddr"\n",
						 curr_lv, save_lv));
					save_lv->ptrs.val_ent.children->lv = save_lv;
					lv_kill(save_lv, FALSE);
					curr_lv->tp_var->var_cloned = FALSE;
				} else
				{	/* If not cloned, we still have to reduce the reference counts of any
					   container vars in the untouched tree that were added to keep anything
					   they referenced from disappearing.
					*/
					DBGRFCT((stderr, "\ntp_unwind: curr_lv not cloned so just reducing ref counts\n"));
					TPUNWND_CNTNRS_IN_TREE(curr_lv);
				}
			}
			LV_FLIST_ENQUEUE(&save_lv->ptrs.val_ent.parent.sym->lv_flist, save_lv);

			/* Not easy to predict what the trefcnt will be except that it should be greater than zero. In
			   most cases, it will have its own hash table ref plus the extras we added but it is also
			   possible that the entry has been kill *'d in which case the ONLY ref that will be left is
			   our own increment but there is no [quick] way to distinguish this case so we just
			   test for > 0.
			*/
			assert(0 < curr_lv->stats.trefcnt);
			assert(0 < curr_lv->stats.crefcnt);
			DECR_CREFCNT(curr_lv);	/* Remove the copy refcnt we added in in op_tstart() or lv_newname() */
			DECR_BASE_REF_NOSYM(curr_lv, FALSE);
			curr_lv->tp_var = NULL;
			tp_pointer->vars = restore_ent->next;
			free(restore_ent);
		}
		if (tp_pointer->fp == frame_pointer && mv_chain->mv_st_type == MVST_TPHOLD && msp == (unsigned char *)mv_chain)
			POP_MV_STENT();
		if (NULL == tp_pointer->old_tp_frame)
			tp_sp = tpstackbase;
		else
			tp_sp = (unsigned char *)tp_pointer->old_tp_frame;
		if (tp_sp > tpstackbase)
			rts_error(VARLSTCNT(1) ERR_STACKUNDERFLO);
		if (tp_pointer->tp_save_all_flg)
			--tp_pointer->sym->tp_save_all;
		if ((NULL != (tp_pointer = tp_pointer->old_tp_frame))	/* Note assignment */
			&& ((tp_pointer < (tp_frame *)tp_sp) || (tp_pointer > (tp_frame *)tpstackbase)
			|| (tp_pointer < (tp_frame *)tpstacktop)))
				rts_error(VARLSTCNT(1) ERR_STACKUNDERFLO);
	}
	if ((0 != newlevel) && restore_lv)
	{	/* Restore current context (without releasing) */
		DBGRFCT((stderr, "\n\n** tp_unwind: Newlevel (%d) != 0 loop processing\n", newlevel));
		for (restore_ent = tp_pointer->vars;  NULL != restore_ent;  restore_ent = restore_ent->next)
		{
			curr_lv = restore_ent->current_value;
			save_lv = restore_ent->save_value;
			assert(curr_lv);
			assert(save_lv);
			assert(MV_SYM == curr_lv->ptrs.val_ent.parent.sym->ident);
			assert(MV_SYM == save_lv->ptrs.val_ent.parent.sym->ident);
			assert(curr_lv->tp_var);
			assert(curr_lv->tp_var == restore_ent);
			assert(0 < curr_lv->stats.trefcnt);
			tp_unwind_restlv(curr_lv, save_lv, restore_ent, &lvscan);
			assert(0 < curr_lv->stats.trefcnt);	/* Should have its own hash table ref plus the extras we added */
			assert(0 < curr_lv->stats.crefcnt);
		}
		/* If we have any lv_vals queued up to be scanned for container vars, do that now */
		DBGRFCT((stderr, "\ntp_unwind: Starting deferred rescan of lv trees needing refcnt processing\n"));
		while (0 < lvscan->elemcnt)
		{
			assert(ARY_SCNCNTNR_DIM >= lvscan->elemcnt);
			for (elemindx = 0; lvscan->elemcnt > elemindx; ++elemindx)
			{
				lv = lvscan->ary_scncntnr[elemindx];
				DBGRFCT((stderr, "\n**tp_unwind_process_lvscan_array: Deferred processing lv 0x"lvaddr"\n", lv));
				assert(MV_SYM == lv->ptrs.val_ent.parent.sym->ident);
				/* This is the final level being restored so redo the counters on these vars */
				TPREST_CNTNRS_IN_TREE(lv);
			}
			/* If we allocated any secondary blocks, we are done with them now so release them. Only the
			   very last block on the chain is the original block that was automatically allocated which
			   should not be freed in this fashion.
			*/
			lvscan_next = lvscan->next;
			if (NULL != lvscan_next)
			{	/* There is another block on the chain so this one can be freed */
				free(lvscan);
				DBGRFCT((stderr, "\ntp_unwind_process_lvscan_array: Freeing lvscan array\n"));
				lvscan = lvscan_next;
			} else
			{	/* Since this is the original block allocated on the C stack which we may reuse,
				   zero the element count.
				*/
				lvscan->elemcnt = 0;
				DBGRFCT((stderr, "\ntp_unwind_process_lvscan_array: Setting elemcnt to 0 in original "
					 "lvscan block\n"));
				assert(lvscan == &first_lvscan);
			}
		}
	}
	assert(0 == lvscan->elemcnt);	/* verify no elements queued that were not scanned */

	rollback_locks = (invocation_type != COMMIT_INVOCATION);
	for (prior = &mlk_pvt_root, mlkp = *prior;  NULL != mlkp;  mlkp = *prior)
	{
		if (mlkp->granted)
		{	/* This was a pre-existing lock */
			for (oldlock = mlkp->tp;  (NULL != oldlock) && ((int)oldlock->tplevel > newlevel);  oldlock = nextlock)
			{	/* Remove references to the lock from levels being unwound */
				nextlock = oldlock->next;
				free(oldlock);
			}
			if (rollback_locks)
			{
				if (NULL == oldlock)
				{	/* Lock did not exist at the tp level being unwound to */
					mlk_unlock(mlkp);
					mlk_pvtblk_delete(prior);
					continue;
				} else
				{	/* Lock still exists but restore lock state as it was when the transaction started. */
					mlkp->level = oldlock->level;
					mlkp->zalloc = oldlock->zalloc;
				}
			}
			if (NULL != oldlock && oldlock->tplevel == newlevel)
			{	/* Remove lock reference from level being unwound to,
				 * now that any {level,zalloc} state information has been restored.
				 */
				assert(NULL == oldlock->next || oldlock->next->tplevel < newlevel);
				mlkp->tp = oldlock->next;	/* update root reference pointer */
				free(oldlock);
			} else
				mlkp->tp = oldlock;	/* update root reference pointer */
			prior = &mlkp->next;
		} else
			mlk_pvtblk_delete(prior);
	}
	DBGRFCT((stderr, "tp_unwind: Processing complete\n"));
	dollar_tlevel = newlevel;
	RESTORE_INTRPT_OK_STATE;	/* check if any MUPIP STOP/signals were deferred while in this function */
}


/* Restore given local variable from supplied TP restore entry into given symval. Note lvscan_anchor will only be non-NULL
   for the final level we are restoring (but not unwinding). We don't need to restore counters for any vars except the
   very last level.
 */
void tp_unwind_restlv(lv_val *curr_lv, lv_val *save_lv, tp_var *restore_ent, lvscan_blk **lvscan_anchor)
{
	ht_ent_mname	*tabent;
	lv_val		*inuse_lv;
	int		elemindx;
	mv_stent	*mvc;
	lvscan_blk	*lvscan, *newlvscan;
	lv_sbs_tbl	*tmpsbs;
	DBGRFCT_ONLY(mident_fixed vname;)

	assert(curr_lv);
	assert(curr_lv->tp_var);
	DBGRFCT_ONLY(
		memcpy(vname.c, restore_ent->key.var_name.addr, restore_ent->key.var_name.len);
		vname.c[restore_ent->key.var_name.len] = '\0';
	);
	DBGRFCT((stderr, "\ntp_unwind_restlv: Entered for varname: '%s' curr_lv: 0x"lvaddr"  save_lv: 0x"lvaddr"\n",
		 &vname.c, curr_lv, save_lv));
	DBGRFCT((stderr, "tp_unwind_restlv: tp_pointer/current: fp: 0x"lvaddr"/0x"lvaddr" mvc: 0x"lvaddr"/0x"lvaddr
		 " symval: 0x"lvaddr"/0x"lvaddr"\n",
		 tp_pointer->fp, frame_pointer, tp_pointer->mvc, mv_chain, tp_pointer->sym, curr_symval));

	/* First get the stack in the position where we can actually process this entry. Need to make sure we are processing
	   the symbol table we need to be processing so unwind enough stuff to get there.
	*/
	if (curr_symval != tp_pointer->sym)
	{	/* Unwind as many stackframes as are necessary up to the max */
		while(curr_symval != tp_pointer->sym && frame_pointer < tp_pointer->fp)
			op_unwind();
		if (curr_symval != tp_pointer->sym)
		{	/* Unwind as many mv_stents as are necessary up to the max */
			mvc = mv_chain;
			while(curr_symval != tp_pointer->sym && mvc < tp_pointer->mvc)
			{

				unw_mv_ent(mvc);
				mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc);
			}
			mv_chain = mvc;
			/* Final check */
			if (curr_symval != tp_pointer->sym)
				GTMASSERT;
		}
	}

	if (curr_lv->tp_var->var_cloned)
	{	/* Var/tree has been copied (and modified) -- see about restoring it */
		DBGRFCT((stderr, "\ntp_unwind_restlv: curr_lv was modified and cloned -- needs restoration\n"));
		if (NULL != restore_ent->key.var_name.addr)
		{	/* Restore data into a named variable (hash table entry) */
			/* Step 1 -- find its hash table address to see what lv_val is there now */
			tabent = lookup_hashtab_mname(&((tp_pointer->sym)->h_symtab), &restore_ent->key);
			assert(tabent);
			/* Step 2 -- If lv_val is NOT the same as it was, then we must replace the lv_val
			   currently in use. Decrement its use count (which will delete it and the tree if
			   it is no longer used) and replace with desired previous lv_val whose use count
			   was incremented when it was saved.
			*/
			if (curr_lv != (inuse_lv = (lv_val *)tabent->value))
			{
				if (inuse_lv)
					DECR_BASE_REF_RQ(tabent, inuse_lv, FALSE);
				DBGRFCT((stderr, "tp_unwind: hte 0x"lvaddr" being reset from 0x"lvaddr" to 0x"lvaddr"\n",
					 tabent, tabent->value, curr_lv));
				tabent->value = (void *)curr_lv;
				INCR_TREFCNT(curr_lv);			/* Back in the hash table, bump its reference */
			}
		} /* Else, if restoring orphaned data, just prune the old var and copy in the saved tree (if one existed) */
		/* Step 3 -- We have the correct lv_val in the hash table now but it has the wrong value.
		   Get rid of its current tree if any.
		*/
		if (tmpsbs = curr_lv->ptrs.val_ent.children)	/* Note assignment */
		{
			DBGRFCT((stderr, "\ntp_unwind_restlv: Killing children of curr_lv 0x"lvaddr"\n", curr_lv));
			assert(curr_lv == tmpsbs->lv);			\
			curr_lv->ptrs.val_ent.children = NULL;	/* prevent recursion due to alias containers */
			lv_killarray(tmpsbs, FALSE);
		}
	} else
		DBGRFCT((stderr, "\ntp_unwind_restlv: curr_lv was NOT modified or cloned\n"));
	/* Step 4:  Copy in the saved flavor lv_val back to curr_lv but preserve the ref counts of
	   the current var since the copy's ref counts have not been kept up to date.
	*/
	assert(0 < curr_lv->stats.trefcnt);
	assert(0 < curr_lv->stats.crefcnt);
	save_lv->stats = curr_lv->stats;	/* Store correct refcnts and cycles in save_lv for correct copy below */
	DBGRFCT((stderr, "\ntp_unwind_restlv: Restoring value of lv 0x"lvaddr" back into lv 0x"lvaddr"\n", save_lv, curr_lv));
	*curr_lv = *save_lv;
	if (NULL == lvscan_anchor)
		/* Means this is completely unwinding a nested level so we need to reset the tstartcycle in this
		   lvval so it gets handled correctly when this lv is encounted again after the restart completes.
		*/
		curr_lv->stats.tstartcycle = 0;
	if (curr_lv->tp_var->var_cloned)
	{	/* Some fixup may need to be done if the variable was cloned (and thus moved around) */
		curr_lv->tp_var->var_cloned = FALSE;
		if (curr_lv->ptrs.val_ent.children)
		{	/* Some pointer fix up needs to be done since the owner of the restored tree changed */
			curr_lv->ptrs.val_ent.children->lv = curr_lv;	/* Pointer fixup */
			if (curr_lv->has_aliascont && NULL != lvscan_anchor)
			{	/* Some ref counts need to be restored for arrays this tree points to -- but only if the
				   array contains pointers (alias containers). */
				DBGRFCT((stderr, "\ntp_unwind_restlv: Putting lv 0x:"lvaddr" on the lvscan list\n", curr_lv));
				/* This array needs to have container pointer target reference counts reestablished. Record
				   the lv so this can happen after all vars are restored.
				*/
				lvscan = *lvscan_anchor;
				elemindx = lvscan->elemcnt++;	/* Note post increment so elemindx has minus-one value */
				if (ARY_SCNCNTNR_MAX < elemindx)
				{	/* Present block is full so allocate a new one and chain it on */
					lvscan->elemcnt--;		/* New element ended up not being in that block.. */
					newlvscan = (lvscan_blk *)malloc(sizeof(lvscan_blk));
					newlvscan->next = lvscan;
					newlvscan->elemcnt = 1;		/* Going to use first one *now* */
					elemindx = 0;
					*lvscan_anchor = newlvscan;
					lvscan = newlvscan;
				}
				assert(ARY_SCNCNTNR_MAX >= elemindx && 0 <= elemindx);
				lvscan->ary_scncntnr[elemindx] = curr_lv;
			}
		}
	}
}
