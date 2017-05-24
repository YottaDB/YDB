/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_signal.h"	/* needed for VSIG_ATOMIC_T */
#include "gtm_stdio.h"
#include "gtm_string.h"

#include <rtnhdr.h>
#include "mv_stent.h"
#include "stack_frame.h"
#include "tp_frame.h"
#include "mlkdef.h"
#include "hashtab_int4.h"	/* needed for tp.h */
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
#include "filestruct.h"
#include "gdscc.h"
#include "have_crit.h"
#include "gdskill.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "jnl.h"
#include "tp.h"
#include "tp_restart.h"
#ifdef UNIX
# include "deferred_signal_handler.h"
#endif
#ifdef GTM_TRIGGER
# include "gv_trigger.h"
# include "gtm_trigger.h"
# include "gt_timer.h"
# include "wbox_test_init.h"
#endif
#ifdef DEBUG
# include "gtmio.h"
# include "gtm_stdio.h"
#endif

GBLREF	stack_frame	*frame_pointer;
GBLREF	unsigned char	*msp, *stackbase, *stacktop;
GBLREF	mv_stent	*mv_chain;
GBLREF	tp_frame	*tp_pointer;
GBLREF	unsigned char	*tp_sp, *tpstackbase, *tpstacktop;
GBLREF	mlk_pvtblk	*mlk_pvt_root;
GBLREF	uint4		dollar_tlevel;
GBLREF	symval		*curr_symval;
GBLREF	uint4		process_id;
GBLREF	sgmnt_addrs	*csa;
GBLREF	lv_val		*active_lv;
#ifdef GTM_TRIGGER
GBLREF	int		tprestart_state;	/* When triggers restart, multiple states possible. See tp_restart.h */
#endif

/* Define whitebox test case as a macro only for a debug build */
#ifdef DEBUG
#  define TPUNWND_WBOX_TEST													 \
{																 \
	if (WBTEST_ENABLED(WBTEST_TRIGR_TPRESTART_MSTOP))									 \
	{	/* For this white box test, we're going to send ourselves a SIGTERM termination signal at a specific point 	 \
		 * in the processing to make sure it succeeds without exploding during rundown. To test the condition GTM-7811	 \
		 * fixes, where TPUNWND_WBOX_TEST is seen, move the reset for dollar_tlevel from before the ENABLE_INTERRUTPS	 \
		 * macro to after the TPUNWND_WBOX_TEST, rebuild and re-run test to see it explode.				 \
		 */														 \
		kill(process_id, SIGTERM);											 \
		hiber_start(20 * 1000);			/* Wait up to 20 secs - don't use wait_any as the heartbeat timer	 \
							 * will kill this wait in 0-7 seconds or so.				 \
							 */									 \
		/* We sent, we waited, wait expired - weird - funky condition is for identification purposes (to identify the	 \
		 * actual assert). We should be dead or dying, not trying to resume.						 \
		 */														 \
		assertpro(WBTEST_TRIGR_TPRESTART_MSTOP == 0);									 \
	}															 \
}
#else
#  define TPUNWND_WBOX_TEST
#endif

error_def(ERR_STACKUNDERFLO);
error_def(ERR_TPRETRY);

void	tp_unwind(uint4 newlevel, enum tp_unwind_invocation invocation_type, int *tprestart_rc)
{
	mlk_pvtblk	**prior, *mlkp;
	mlk_tp		*oldlock, *nextlock;
	int		tl;
	lv_val		*save_lv, *curr_lv, *lv;
	tp_var		*restore_ent;
	mv_stent	*mvc;
	boolean_t	restore_lv, rollback_locks;
	int		elemindx, rc;
	lvTree		*lvt_child;
	intrpt_state_t	prev_intrpt_state;

	/* We are about to clean up structures. Defer MUPIP STOP/signal handling until function end. */
	DEFER_INTERRUPTS(INTRPT_IN_TP_UNWIND, prev_intrpt_state);
	/* Unwind the requested TP levels */
#	if defined(DEBUG_REFCNT) || defined(DEBUG_ERRHND)
	DBGFPF((stderr, "\ntp_unwind: Beginning TP unwind process - reason: %d\n", invocation_type));
#	endif
	assert((COMMIT_INVOCATION == invocation_type) || (RESTART_INVOCATION == invocation_type)
			|| (ROLLBACK_INVOCATION == invocation_type));
	/* If restarting or rolling back, clear active_lv (if it is non-NULL now) since it was NULL at TSTART time. */
	if (RESTART_INVOCATION == invocation_type)
	{
		UNDO_ACTIVE_LV(actlv_tp_unwind_restart);
	} else if (ROLLBACK_INVOCATION == invocation_type)
	{
		UNDO_ACTIVE_LV(actlv_tp_unwind_rollback);
	} else
	{
		ASSERT_ACTIVELV_GOOD(active_lv);
		if (0 == newlevel)
		{	/* outermost TCOMMIT. Restore active_lv to what it was at implicit TSTART time if it was non-NULL */
			assert(NULL != tp_pointer);
			assert(NULL == tp_pointer->old_tp_frame);
			if (tp_pointer->implicit_tstart)
			{
				lv = tp_pointer->active_lv;
				if (NULL != lv)
					SET_ACTIVE_LV(lv, FALSE, actlv_tp_unwind_commit);
			}
		}
	}
	restore_lv = (RESTART_INVOCATION == invocation_type);
	assert((tp_sp <= tpstackbase) && (tp_sp > tpstacktop));
	assert((tp_pointer <= (tp_frame *)tpstackbase) && (tp_pointer > (tp_frame *)tpstacktop));
	for (tl = dollar_tlevel; tl > newlevel; --tl)
	{
		DBGRFCT((stderr, "\ntp_unwind: Unwinding level %d -- tp_pointer: 0x"lvaddr"\n", tl, tp_pointer));
		assertpro(NULL != tp_pointer);
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
			assert(LV_IS_BASE_VAR(curr_lv));
			assert(LV_IS_BASE_VAR(save_lv));
			assert(0 < curr_lv->stats.trefcnt);
			assert(curr_lv->tp_var);
			assert(curr_lv->tp_var == restore_ent);
			DBGRFCT((stderr, "\ntp_unwind: Top of restore-ent loop - curr_lv: 0x"lvaddr"   save_lv: 0x"lvaddr
				 "  restore_ent: 0x"lvaddr"\n", curr_lv, save_lv, restore_ent));
			/* In order to restart sub-transactions, this would have to maintain
			 * the chain that currently is not built by op_tstart()
			 */
			if (restore_lv)
			{
				DBGRFCT((stderr, "\ntp_unwind: Need to restore curr_lv which has been copied\n"));
				rc = tp_unwind_restlv(curr_lv, save_lv, restore_ent, TRUE, tprestart_rc);
#				ifdef GTM_TRIGGER
				if (0 != rc)
				{
					dollar_tlevel = tl;			/* Record fact if we unwound some tp_frames */
					ENABLE_INTERRUPTS(INTRPT_IN_TP_UNWIND, prev_intrpt_state);	/* drive any MUPIP STOP
													 * or signals deferred
													 * while in this function */
					TPUNWND_WBOX_TEST;			/* Debug-only wbox-test to simulate SIGTERM */
					INVOKE_RESTART;
				}
#				endif
			} else if (restore_ent->var_cloned)
			{	/* curr_lv has been cloned.
				 * Note: LV_CHILD(save_lv) can be non-NULL only if restore_ent->var_cloned is TRUE
				 */
				DBGRFCT((stderr, "\ntp_unwind: Not restoring curr_lv and is cloned\n"));
				lvt_child = LV_GET_CHILD(save_lv);
				if (NULL != lvt_child)
				{	/* If subtree exists, we have to blow away the cloned tree */
					DBGRFCT((stderr, "\ntp_unwind: save_lv has children\n"));
					assert(save_lv->tp_var);
					DBGRFCT((stderr,"\ntp_unwind: For lv_val 0x"lvaddr": Deleting saved lv_val 0x"lvaddr"\n",
						 curr_lv, save_lv));
					assert(LVT_PARENT(lvt_child) == (lvTreeNode *)save_lv);
					lv_kill(save_lv, DOTPSAVE_FALSE, DO_SUBTREE_TRUE);
				}
				restore_ent->var_cloned = FALSE;
			} /* Else, if was not cloned, there's nothing more to do for this entry except common refcnt maint below
			   * since any var we visited at TSTART has its own restore_ent to process it on the way out.
			   */
			LV_FREESLOT(save_lv);
			/* Not easy to predict what the trefcnt will be except that it should be greater than zero. In
			 * most cases, it will have its own hash table ref plus the extras we added but it is also
			 * possible that the entry has been kill *'d in which case the ONLY ref that will be left is
			 * our own increment but there is no [quick] way to distinguish this case so we just
			 * test for > 0.
			 */
			assert(0 < curr_lv->stats.trefcnt);
			assert(0 < curr_lv->stats.crefcnt);
			DECR_CREFCNT(curr_lv);			/* Remove refcnt bumps added in in op_tstart() or lv_newname() */
			DECR_BASE_REF_NOSYM(curr_lv, FALSE);
			curr_lv->tp_var = NULL;
			curr_lv->stats.tstartcycle = 0;		/* As if had done nothing with var in this sub-transaction */
			tp_pointer->vars = restore_ent->next;
			free(restore_ent);
		}
		if ((tp_pointer->fp == frame_pointer) && (MVST_TPHOLD == mv_chain->mv_st_type)
		    && (msp == (unsigned char *)mv_chain))
			POP_MV_STENT();
		if (NULL == tp_pointer->old_tp_frame)
			tp_sp = tpstackbase;
		else
			tp_sp = (unsigned char *)tp_pointer->old_tp_frame;
		if (tp_sp > tpstackbase)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_STACKUNDERFLO);
		if (tp_pointer->tp_save_all_flg)
			--tp_pointer->sym->tp_save_all;
		if ((NULL != (tp_pointer = tp_pointer->old_tp_frame))	/* Note assignment */
		    && ((tp_pointer < (tp_frame *)tp_sp) || (tp_pointer > (tp_frame *)tpstackbase)
			|| (tp_pointer < (tp_frame *)tpstacktop)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_STACKUNDERFLO);
	}
	if ((0 != newlevel) && restore_lv)
	{	/* Restore current context (without releasing) */
		assertpro(NULL != tp_pointer);
		DBGRFCT((stderr, "\n\n** tp_unwind: Restore target unwind TP level (level %d) local vars - tp_pointer = 0x"lvaddr
			 "\n", newlevel, tp_pointer));
		for (restore_ent = tp_pointer->vars;  NULL != restore_ent;  restore_ent = restore_ent->next)
		{
			curr_lv = restore_ent->current_value;
			save_lv = restore_ent->save_value;
			assert(curr_lv);
			assert(save_lv);
			assert(LV_IS_BASE_VAR(curr_lv));
			assert(LV_IS_BASE_VAR(save_lv));
			assert(curr_lv->tp_var);
			assert(curr_lv->tp_var == restore_ent);
			assert(0 < curr_lv->stats.trefcnt);
			rc = tp_unwind_restlv(curr_lv, save_lv, restore_ent, FALSE, tprestart_rc);
#			ifdef GTM_TRIGGER
			if (0 != rc)
			{
				dollar_tlevel = tl;			/* Record fact if we unwound some levels */
				ENABLE_INTERRUPTS(INTRPT_IN_TP_UNWIND, prev_intrpt_state); /* drive any MUPIP STOP/signals
											    * deferred while in this function */
				TPUNWND_WBOX_TEST;			/* Debug-only wbox-test to simulate SIGTERM */
				INVOKE_RESTART;
			}
#			endif
			assert(0 < curr_lv->stats.trefcnt);	/* Should have its own hash table ref plus the extras we added */
			assert(0 < curr_lv->stats.crefcnt);
		}
	}
	rollback_locks = (COMMIT_INVOCATION != invocation_type);
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
			if ((NULL != oldlock) && (oldlock->tplevel == newlevel))
			{	/* Remove lock reference from level being unwound to,
				 * now that any {level,zalloc} state information has been restored.
				 */
				assert((NULL == oldlock->next) || (oldlock->next->tplevel < newlevel));
				mlkp->tp = oldlock->next;	/* update root reference pointer */
				free(oldlock);
			} else
				mlkp->tp = oldlock;	/* update root reference pointer */
			prior = &mlkp->next;
		} else
			mlk_pvtblk_delete(prior);
	}
	DBGRFCT((stderr, "tp_unwind: Processing complete\n"));
	/* Reset tprestart_state here instead of (or in addition to) the regular places it is modified because this routine
	 * is used both in normal TP unwind scenarios as well as in error conditions and is part of running down the process
	 * in the exit handler. Added due to the need for a statsDB to kill a record from its DB as part of it being rundown
	 * and the first need to NOT be in the middle of a TP transaction when that happens.
	 */
	if ((0 == newlevel) && (NULL == tp_pointer))
		tprestart_state = TPRESTART_STATE_NORMAL;
	dollar_tlevel = newlevel;
	ENABLE_INTERRUPTS(INTRPT_IN_TP_UNWIND, prev_intrpt_state);/* drive any MUPIP STOP/signals deferred while in this function */
}


/* Restore given local variable from supplied TP restore entry into given symval. Note clearTStartCycle is FALSE
 * for the final level we are restoring (but not unwinding).
 *
 * The return code is only used when unrolling the M stack runs into a trigger base frame which must be unrolled
 * by gtm_trigger. A non-zero return code signals to tp_unwind() that it needs to rethrow the tprestart error.
 */
int tp_unwind_restlv(lv_val *curr_lv, lv_val *save_lv, tp_var *restore_ent, boolean_t clearTStartCycle, int *tprestart_rc)
{
	ht_ent_mname	*tabent;
	lv_val		*inuse_lv;
	int		elemindx;
	mv_stent	*mvc;
	lvTree		*lvt_child;
	boolean_t	var_cloned;

	assert(curr_lv);
	assert(LV_IS_BASE_VAR(curr_lv));
	assert(curr_lv->tp_var);
	DBGRFCT((stderr, "\ntp_unwind_restlv: Entered for varname: '%.*s' curr_lv: 0x"lvaddr"  save_lv: 0x"lvaddr
		 "  restore_ent: 0x"lvaddr"\n", restore_ent->key.var_name.len, restore_ent->key.var_name.addr, curr_lv, save_lv,
		 restore_ent));
	DBGRFCT((stderr, "tp_unwind_restlv: tp_pointer->fp/frame_pointer: 0x"lvaddr"/0x"lvaddr" tp_pointer->mvc/mv_chain: 0x"
		 lvaddr"/0x"lvaddr" tp_pointer->symval/curr_symval: 0x"lvaddr"/0x"lvaddr"\n",
		 tp_pointer->fp, frame_pointer, tp_pointer->mvc, mv_chain, tp_pointer->sym, curr_symval));

	/* First get the stack in the position where we can actually process this entry. Need to make sure we are processing
	 * the symbol table we need to be processing so unwind enough stuff to get there.
	 */
	if (curr_symval != tp_pointer->sym)
	{	/* Unwind as many stackframes as are necessary up to the max */
		while((curr_symval != tp_pointer->sym) && (frame_pointer < tp_pointer->fp))
		{
#			ifdef GTM_TRIGGER
			if (SFT_TRIGR & frame_pointer->type)
			{	/* We have encountered a trigger base frame. We cannot unroll it because there are C frames
				 * associated with it so we must interrupt this tp_restart and return to gtm_trigger() so
				 * it can unroll the base frame and rethrow the error to properly unroll the C stack.
				 */
				*tprestart_rc = ERR_TPRETRY;
				tprestart_state = TPRESTART_STATE_TPUNW;
				DBGTRIGR((stderr, "tp_unwind: Encountered trigger base frame during M-stack unwind - "
					  "rethrowing\n"));
				return -1;
			}
#			endif
			op_unwind();
		}
		if (curr_symval != tp_pointer->sym)
		{	/* Unwind as many mv_stents as are necessary up to the max */
			mvc = mv_chain;
			while((curr_symval != tp_pointer->sym) && (mvc < tp_pointer->mvc))
			{
				unw_mv_ent(mvc);
				mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc);
			}
			mv_chain = mvc;
			/* Final check */
			assertpro(curr_symval == tp_pointer->sym);
		}
	}
	var_cloned = curr_lv->tp_var->var_cloned;
	if (var_cloned)
	{	/* Var/tree has been copied (and modified) -- see about restoring it */
		DBGRFCT((stderr, "\ntp_unwind_restlv: curr_lv was modified and cloned -- needs restoration\n"));
		if (NULL != restore_ent->key.var_name.addr)
		{	/* Restore data into a named variable (hash table entry)
			 * Step 1 -- find its hash table address to see what lv_val is there now.
			 */
			tabent = lookup_hashtab_mname(&((tp_pointer->sym)->h_symtab), &restore_ent->key);
			assert(tabent);
			/* Step 2 -- If lv_val is NOT the same as it was, then we must replace the lv_val
			 * currently in use. Decrement its use count (which will delete it and the tree if
			 * it is no longer used) and replace with desired previous lv_val whose use count
			 * was incremented when it was saved.
			 */
			if (curr_lv != (inuse_lv = (lv_val *)tabent->value))	/* Note assignment */
			{
				if (inuse_lv)
					DECR_BASE_REF_RQ(tabent, inuse_lv, FALSE);
				DBGRFCT((stderr, "tp_unwind_restlv: hte 0x"lvaddr" being reset from 0x"lvaddr" to 0x"lvaddr
					 " in %s at line %d\n", tabent, tabent->value, curr_lv, __FILE__, __LINE__));
				tabent->value = (void *)curr_lv;
				INCR_TREFCNT(curr_lv);			/* Back in the hash table, bump its reference */
			}
		} /* Else, if restoring orphaned data, just prune the old var and copy in the saved tree (if one existed) */
		/* Step 3 -- We have the correct lv_val in the hash table now but it has the wrong value.
		 * Get rid of its current tree if any.
		 */
		if (lvt_child = LV_GET_CHILD(curr_lv))	/* Note assignment */
		{
			DBGRFCT((stderr, "\ntp_unwind_restlv: Killing child tree of curr_lv 0x"lvaddr"\n", curr_lv));
			assert((lvTreeNode *)curr_lv == LVT_PARENT(lvt_child));
			LV_CHILD(curr_lv) = NULL;	/* prevent recursion due to alias containers */
			lv_killarray(lvt_child, FALSE);
		}
		/* Step 4:  Copy in the needed fields from the saved flavor lv_val back to curr_lv.
		 * Preserve the ref counts of the current var since the copy's ref counts have not been kept up to date.
		 */
		DBGRFCT((stderr, "\ntp_unwind_restlv: Restoring value of lv 0x"lvaddr" back into lv 0x"lvaddr"\n",
			save_lv, curr_lv));
		/* The following is optimized to do the initialization of just the needed structure members. For that it assumes a
		 * particular "lv_val" structure layout. The assumed layout is asserted so any changes to the layout will
		 * automatically show an issue here and cause the below initialization to be accordingly reworked.
		 */
		assert(0 == OFFSETOF(lv_val, v));
		assert(OFFSETOF(lv_val, v) + SIZEOF(curr_lv->v) == OFFSETOF(lv_val, ptrs));
		assert(OFFSETOF(lv_val, ptrs) + SIZEOF(curr_lv->ptrs) == OFFSETOF(lv_val, stats));
		assert(OFFSETOF(lv_val, stats) + SIZEOF(curr_lv->stats) == OFFSETOF(lv_val, has_aliascont));
		assert(OFFSETOF(lv_val, has_aliascont) + SIZEOF(curr_lv->has_aliascont) == OFFSETOF(lv_val, lvmon_mark));
		assert(OFFSETOF(lv_val, lvmon_mark) + SIZEOF(curr_lv->lvmon_mark) == OFFSETOF(lv_val, tp_var));
		assert(OFFSETOF(lv_val, tp_var) + SIZEOF(curr_lv->tp_var) == SIZEOF(lv_val));
		/* save_lv -> curr_lv Copy begin */
		curr_lv->v = save_lv->v;
		curr_lv->ptrs = save_lv->ptrs;
		assert(0 < curr_lv->stats.trefcnt);	/* No need to copy "stats" as curr_lv is more uptodate */
		assert(0 < curr_lv->stats.crefcnt);
		assert(8 == (OFFSETOF(lv_val, tp_var) - OFFSETOF(lv_val, has_aliascont)));
		curr_lv->has_aliascont = save_lv->has_aliascont;
		DBGALS_ONLY(curr_lv->lvmon_mark = save_lv->lvmon_mark);
		assert(save_lv->tp_var == curr_lv->tp_var);	/* no need to copy this field */
		/* save_lv -> curr_lv Copy done */
		/* Some fixup may need to be done if the variable was cloned (and thus moved around) */
		curr_lv->tp_var->var_cloned = FALSE;
		if (lvt_child = LV_GET_CHILD(curr_lv))
		{	/* Some pointer fix up needs to be done since the owner of the restored tree changed */
			assert(LVT_PARENT(lvt_child) == ((lvTreeNode *)curr_lv->tp_var->save_value));
			LV_CHILD(save_lv) = NULL;	/* now that curr_lv->tp_var->var_cloned has been reset */
			LVT_PARENT(lvt_child) = (lvTreeNode *)curr_lv;
		}
	} else
	{
		DBGRFCT((stderr, "\ntp_unwind_restlv: curr_lv was NOT modified or cloned\n"));
		assert(NULL == LV_CHILD(save_lv));
		assert(!save_lv->tp_var->var_cloned);
		/* We know that the subscript array underneath curr_lv did not change since saving it into save_lv. But the
		 * unsubscripted lv could have changed (have no way of checking if that is the case) so restore it (just its
		 * value) unconditionally.
		 */
		curr_lv->v = save_lv->v;
		/* No need to copy "save_lv->ptrs" as "ptrs" contains 2 fields both of which are already correct in "curr_lv" */
		assert(save_lv->ptrs.val_ent.parent.sym == curr_lv->ptrs.val_ent.parent.sym);
		assert(NULL == save_lv->ptrs.val_ent.children);
		/* No need to copy "save_lv->stats" as "curr_lv->stats" is more uptodate */
		assert(save_lv->has_aliascont == curr_lv->has_aliascont);	/* No need to copy this field */
		assert(save_lv->lvmon_mark == curr_lv->lvmon_mark);		/* No need to copy this field */
		assert(save_lv->tp_var == curr_lv->tp_var);			/* No need to copy this field */
	}
	if (clearTStartCycle)
		/* Means this is completely unwinding a nested level so we need to reset the tstartcycle in this
		 * lvval so it gets handled correctly when this lv is encountered again after the restart completes.
		 */
		curr_lv->stats.tstartcycle = 0;
	DBGRFCT((stderr, "tp_unwind_restlv: Complete (returning)\n"));
	return 0;
}
