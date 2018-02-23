/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Code in this module contains parts and pieces of sr_port/mdb_condition_handler and hence has an
 * FIS copyright even though this module was not created by FIS.
 */

#include "mdef.h"

#include "gtm_string.h"

#include "error.h"
#include "error_trap.h"
#include "lv_val.h"
#include "hashtab_mname.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "zwrite.h"
#include "change_reg.h"
#include "tp_change_reg.h"
#include "dpgbldir.h"
#include "ftok_sems.h"
#include "gv_trigger.h"
#include "gtm_trigger.h"
#include "repl_msg.h"
#include "preemptive_db_clnup.h"
#include "filestruct.h"
#include "gtmsource.h"
#include "buddy_list.h"
#include "gdscc.h"
#include "hashtab_int4.h"
#include "jnl.h"
#include "gdskill.h"
#include "tp.h"
#include "gtmdbglvl.h"

GBLREF	stack_frame		*frame_pointer;
GBLREF	boolean_t		created_core;
GBLREF	boolean_t		dont_want_core;
GBLREF	boolean_t		in_gvcst_incr;
GBLREF	boolean_t		need_core;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	sgm_info		*first_sgm_info;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF  dollar_ecode_type 	dollar_ecode;
GBLREF	jnlpool_addrs_ptr_t	jnlpool_head;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	uint4			gtmDebugLevel;		/* Debug level */
GBLREF	mval			dollar_zstatus;

/* Condition handler for simpleAPI environment. This routine catches all errors thrown by the YottaDB engine. The error
 * is basically returned to the user as the negative of the error to differentiate those errors from positive (success
 * or informative) return codes of this API.
 */
CONDITION_HANDLER(ydb_simpleapi_ch)
{
	sgmnt_addrs		*csa;
	unix_db_info		*udi;
	gd_addr			*addr_ptr;
	gd_region		*reg_top, *reg_local;
	jnlpool_addrs_ptr_t	local_jnlpool;

	START_CH(TRUE);
	if (ERR_REPEATERROR == SIGNAL)
		arg = SIGNAL = dollar_ecode.error_last_ecode;	/* Rethrown error. Get primary error code */
	/* The mstrs that were part of the current ydb_*_s() call and were being protected from "stp_gcol" through a global
	 * array no longer need that protection since we are about to exit from the ydb_*_s() call. So clear the global array index.
	 */
	preemptive_db_clnup(SEVERITY);
	in_gvcst_incr = FALSE;    /* reset this just in case gvcst_incr/gvcst_put failed to do a good job of resetting */
	if ((SUCCESS != SEVERITY) && (INFO != SEVERITY))
	{
		inctn_opcode = inctn_invalid_op;
		/* Ideally merge should have a condition handler to reset followings, but generated code can call other routines
		 * during MERGE command (MERGE command invokes multiple op-codes depending on source vs target). So it is not
		 * easy to establish a condition handler there. Easy solution is following one line code.
		 */
		NULLIFY_MERGE_ZWRITE_CONTEXT;
	}
	if (ERR_REPEATERROR != SIGNAL)
		set_zstatus(NULL, arg, NULL, FALSE);
	TREF(ydb_error_code) = arg;			/* Record error code for caller */
	/* Ensure gv_target and cs_addrs are in sync. If not, make them so. */
	if (NULL != gv_target)
	{
		csa = gv_target->gd_csa;
		if (NULL != csa)
		{
			if (csa != cs_addrs)
			{
				assert(0 < csa->regcnt);
				/* If csa->regcnt is > 1, it is possible that csa->region is different from the actual gv_cur_region
				 * (before we encountered the runtime error). This is a case of two regions mapping to the same csa.
				 * The only issue with this is that some user-level error messages that have the region name (as
				 * opposed to the database file name) could print incorrect values. But other than that there should
				 * be no issues since finally the csa (corresponding to the physical database file) is what matters
				 * and that is the same for both the regions. Given that the region mismatch potential exists only
				 * until the next global reference which is different from $REFERENCE, we consider this acceptable.
				 */
				gv_cur_region = csa->region;
				assert(gv_cur_region->open);
				assert(IS_REG_BG_OR_MM(gv_cur_region));
				/* The above assert is needed to ensure that change_reg/tp_change_reg (invoked below)
				 * will set cs_addrs, cs_data etc. to non-zero values.
				 */
				if (NULL != first_sgm_info)
					change_reg(); /* updates "cs_addrs", "cs_data", "sgm_info_ptr" and maybe "first_sgm_info" */
				else
				{	/* We are either inside a non-TP transaction or a TP transaction that has done NO database
					 * references. In either case, we do NOT want to setting sgm_info_ptr or first_sgm_info.
					 * Hence use tp_change_reg instead of change_reg below.
					 */
					tp_change_reg(); /* updates "cs_addrs", "cs_data" */
				}
				assert(cs_addrs == csa);
				assert(cs_data == csa->hdr);
				assert(NULL != cs_data);
			}
			/* Fix gv_currkey to null-str in case gv_target points to dir_tree (possible in case of name-level-$order).
			 * Do same in case gv_target points to cs_addrs->hasht_tree so we dont take the fast path in op_gvname
			 * when gv_target is clearly not GVT of a user-visible global.
			 */
			if ((gv_target == csa->dir_tree) GTMTRIG_ONLY(|| (gv_target == csa->hasht_tree)))
			{
				gv_currkey->end = 0;
				gv_currkey->base[0] = 0;
			}
		}
	}
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	/* If this is a fatal error, create a core file and close up shop so the runtime cannot be called again.
	 */
	if ((DUMPABLE) && !SUPPRESS_DUMP)
	{	/* Fatal errors need to create a core dump */
		process_exiting = TRUE;
		CANCEL_TIMERS;
		if (!(GDL_DumpOnStackOFlow & gtmDebugLevel) &&
		    ((int)ERR_STACKOFLOW == SIGNAL || (int)ERR_STACKOFLOW == arg
		     || (int)ERR_MEMORY == SIGNAL || (int)ERR_MEMORY == arg))
		{
			dont_want_core = TRUE;		/* Do a clean exit rather than messy core exit */
			need_core = FALSE;
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_FATALERROR1, 2, RTS_ERROR_MVAL(&dollar_zstatus));
		} else
		{
			need_core = TRUE;
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_FATALERROR2, 2, RTS_ERROR_MVAL(&dollar_zstatus));
		}
		TERMINATE;
	}
	UNDO_ACTIVE_LV(actlv_ydb_simpleapi_ch);
	/*
	 * If error is at least severity "WARNING", do some cleanups. Note: crit is no longer unconditionally
	 * released here. It is now released if NOT in TP (any retry) or if in TP but NOT in the final retry.
	 * But we still cleanup the replication instance file lock and the replication crit lock. We do this
	 * because when starting the final retry GTM only holds the grab_crit locks but not the grab_lock or
	 * ftok_sem_lock locks. The grab_lock is done only at commit time (at which point there cannot be any
	 * programmatic runtime errors possible). Errors inside jnlpool_init (which cause the ftok-sem-lock
	 * to be held) can never be programmatically caused.
	 *
	 * On the other hand, while running in the final retry holding crit, a program can cause an arbitrary
	 * error (e.g. 1/0 error or similar). Hence the distinction. The reasoning is that any of these could
	 * potentially take an arbitrary amount of time and we don't want to be holding critical locks while
	 * doing these. But one can argue that the error trap is nothing but an extension of the transaction
	 * and that if GT.M is fine executing arbitrary application M code in the final retry holding crit,
	 * it should be fine doing the same for the error trap M code as well. So the entire point of not
	 * releasing crit is to avoid indefinite TP retries due to runtime errors. But this means the error
	 * trap should be well coded (i.e. not have any long running commands etc.) to avoid crit hangs
	 * and/or deadlocks.
	 */
	if ((SUCCESS != SEVERITY) && (INFO != SEVERITY))
	{
		if ((0 == dollar_tlevel) || (CDB_STAGNATE > t_tries))
		{	/* Only release crit if we are NOT in TP *or* if we are in TP, we aren't in final retry */
			for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
			{
				for (reg_local = addr_ptr->regions, reg_top = reg_local + addr_ptr->n_regions;
				     reg_local < reg_top; reg_local++)
				{
					if (reg_local->open && !reg_local->was_open)
					{
						csa = (sgmnt_addrs *)&FILE_INFO(reg_local)->s_addrs;
						if (csa && csa->now_crit)
						{
							assert(!csa->hold_onto_crit UNIX_ONLY(|| jgbl.onlnrlbk));
							if (csa->hold_onto_crit)
								csa->hold_onto_crit = FALSE; /* Fix it in pro */
							rel_crit(reg_local);
						}
					}
				}
			}
		}
		/* Release FTOK lock on the replication instance file if holding it (possible if error in jnlpool_init) */
		for (local_jnlpool = jnlpool_head; local_jnlpool; local_jnlpool = local_jnlpool->next)
		{
			if ((NULL != local_jnlpool->jnlpool_dummy_reg) && local_jnlpool->jnlpool_dummy_reg->open)
			{
				udi = FILE_INFO(local_jnlpool->jnlpool_dummy_reg);
				assert(NULL != udi);
				if (NULL != udi)
				{
					if (udi->grabbed_ftok_sem)
						ftok_sem_release(local_jnlpool->jnlpool_dummy_reg, FALSE, FALSE);
					assert(!udi->grabbed_ftok_sem);
				}
			}
		}
		/* Release crit lock on journal pool if holding it */
		for (local_jnlpool = jnlpool_head; local_jnlpool; local_jnlpool = local_jnlpool->next)
			if (local_jnlpool->pool_init)
			{
				csa = (sgmnt_addrs *)&FILE_INFO(local_jnlpool->jnlpool_dummy_reg)->s_addrs;
				if (csa && csa->now_crit)
					rel_lock(local_jnlpool->jnlpool_dummy_reg);
			}
		TREF(in_op_fnnext) = FALSE;			/* in case we were in $NEXT */
		/* Global values that may need cleanups */
		if (INTRPT_OK_TO_INTERRUPT != intrpt_ok_state)
			ENABLE_INTERRUPTS(intrpt_ok_state, INTRPT_OK_TO_INTERRUPT);	/* If interrupts were deferred,
											 * re-enable them now */
	}
#	ifdef GTM_TRIGGER
	/* At this point, we are past the point where the frame pointer is allowed to be resting on a trigger frame
	 * (this is possible in a TPRETRY situation where gtm_trigger must return to gtm_trigger() signaling a
	 * restart is necessary). If we are on a trigger base frame, unwind it so the error is recognized in
	 * the invoker's frame or we unwind back to the call-in base frame being used by the simpleapi.
	 */
	if (SFT_TRIGR & frame_pointer->type)
	{
		/* Better be an error in here info or success messages want to continue, not be unwound */
		assert((SUCCESS != SEVERITY) && (INFO != SEVERITY));
		gtm_trigger_fini(TRUE, FALSE);
	}
#	endif
	TREF(sapi_mstrs_for_gc_indx) = 0;
	TREF(sapi_query_node_subs_cnt) = 0;
	/* If this message was SUCCESS or INFO, just return to caller. Else UNWIND back to where we were established */
	if ((SUCCESS == SEVERITY) || (INFO == SEVERITY))
		CONTINUE;
	UNWIND(NULL, NULL); 		/* Return back to ESTABLISH_NORET() in caller */
}
