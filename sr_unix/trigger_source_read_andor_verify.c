/****************************************************************
 *								*
 *	Copyright 2011, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef GTM_TRIGGER
#include "error.h"
#include "gdsroot.h"			/* for gdsfhead.h */
#include "gdsbt.h"			/* for gdsfhead.h */
#include "gdsfhead.h"
#include "gvcst_protos.h"
#include <rtnhdr.h>
#include "gv_trigger.h"
#include "gtm_trigger.h"
#include "trigger.h"
#include "trigger_fill_xecute_buffer.h"
#include "trigger_gbl_fill_xecute_buffer.h"
#include "trigger_read_name_entry.h"
#include "trigger_source_read_andor_verify.h"
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include "hashtab.h"			/* for STR_HASH (in COMPUTE_HASH_MNAME) */
#include "targ_alloc.h"			/* for SETUP_TRIGGER_GLOBAL & SWITCH_TO_DEFAULT_REGION */
#include "filestruct.h"			/* for INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED (FILE_INFO) */
#include "mvalconv.h"
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "hashtab_int4.h"		/* needed for tp.h */
#include "jnl.h"			/* needed for tp.h */
#include "tp.h"				/* for sgm_info */
#include "tp_frame.h"
#include "tp_restart.h"
#include "tp_set_sgm.h"
#include "t_retry.h"
#include "op.h"
#include "op_tcommit.h"
#include "memcoherency.h"
#include "gtmimagename.h"
#include "cdb_sc.h"
#include "mv_stent.h"
#include "gv_trigger_protos.h"

GBLREF	uint4			dollar_tlevel;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_addr			*gd_header;
GBLREF	gv_key			*gv_currkey;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	gv_namehead		*gv_target;
GBLREF	int			tprestart_state;
GBLREF	tp_frame		*tp_pointer;
GBLREF	int4			gtm_trigger_depth;
GBLREF	trans_num		local_tn;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
#ifdef DEBUG
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
#endif

LITREF	mval			literal_batch;
LITREF	mval			literal_hasht;

#define TRIG_FAILURE_RC	-1

STATICFNDCL CONDITION_HANDLER(trigger_source_raov_ch);
STATICFNDCL int trigger_source_raov(mstr *trigname, trigger_action trigger_op);
STATICFNDCL int trigger_source_raov_tpwrap_helper(mstr *trigname, trigger_action trigger_op);
STATICFNDCL boolean_t trigger_source_raov_trigload(mstr *trigname, gv_trigger_t **ret_trigdsc);

error_def(ERR_DBROLLEDBACK);
error_def(ERR_TPRETRY);
error_def(ERR_TRIGCOMPFAIL);
error_def(ERR_TRIGNAMBAD);
error_def(ERR_TRIGNAMENF);

/* If we have an implicit transaction and are about to fire an error, commit the transaction first so we can
 * get rid of the transaction connotation before error handling gets involved. Note we use op_tcommit() here
 * instead of op_trollback so we can verify the conditions that generated the error. If some restartable
 * condition caused the error, this will restart and retry the transaction. Note that since skip_INVOKE_RESTART
 * is not set before this op_tcommit, it with throw a restart rather than returning a restartable code.
 */
#define CLEAR_IMPLICIT_TP_BEFORE_ERROR						\
	if (tp_pointer->implicit_trigger && (0 == gtm_trigger_depth))		\
	{	/* We have an implicit TP fence */				\
		enum cdb_sc		status;					\
		/* Eliminate transaction by commiting it (nothing was done) */	\
		status = op_tcommit();						\
		assert(cdb_sc_normal == status);				\
	}


/* Similar condition handler to gvtr_tpwrap_ch except we don't insist on first_sgm_info being set */
CONDITION_HANDLER(trigger_source_raov_ch)
{
	int	rc;

	START_CH;
	if ((int)ERR_TPRETRY == SIGNAL)
	{
		/* This only happens at the outer-most TP layer so state should be normal now */
		assert(TPRESTART_STATE_NORMAL == tprestart_state);
		tprestart_state = TPRESTART_STATE_NORMAL;
		rc = tp_restart(1, !TP_RESTART_HANDLES_ERRORS);
		assert(0 == rc);
		assert(TPRESTART_STATE_NORMAL == tprestart_state);	/* No rethrows possible */
		DBGTRIGR((stderr, "trigger_source_ch: Unwinding due to TP REstart\n"));
		UNWIND(NULL, NULL);
	}
	NEXTCH;
}

/* Routine to check on the named trigger. This routine is called from (at least) two places: the above get_src_line()
 * routine where it is called to retrieve trigger source (left in source definition buffer of the trigger) and the other
 * time it is called from op_setzbrk() to load and/or compile the trigger. In both cases, the trigger may be loaded or
 * its source loaded already so we perform a validation on it that it is the current trigger. If the trigger contents are
 * stale, Our actions depend on what mode we are in. If already in TP, we cause the trigger to be unloaded and signal a
 * restart. If not in TP, we just unload the trigger and work our full mojo on it to reload the current version.
 *
 * The following sitations can exist:
 *
 *   1. No trigger by the given name is loaded. For this situation, we need to locate and load the trigger and its source.
 *   2. Trigger is loaded but no source is in the trigger source buffer. For this situation, verify the trigger load is
 *      current. If not, restart things. If is current, load the source.
 *   3. Trigger and source both loaded. Verify the trigger is current. if not restart things.
 *
 * In addition, we can be entered either with a TP FENCE already enabled or without one. How we deal with restarts varries
 * depending on which is true:
 *
 *   - If in a TP fence already, if we hit a condition where we need to restart, we throw a trigger based restart condition
 *     but because we aren't necessarily driving any triggers here, there is nothing in the restart process that actually
 *     forces the trigger to reload before we come back here. So we call gvtr_free() on the region in question to force
 *     those triggers to reload completely, even if it is us that ends up doing it when we get back here.
 *   - If NOT under a TP fence already, we provide an implcit wrapper that will catch our restarts and reinvoke the logic
 *     that will reload the trigger from scratch.
 *
 * Note, this routine is for loading trigger source when we are not driving triggers. The trigger_fill_xecute_buffer()
 * should be used when fetching source for trigger execution because it is lighter weight with built-in trigger refetch
 * logic since we are using the globals the triggers live in. In this case, the trigger access is adhoc for the $TEXT()
 * ZPRINT and ZBREAK uses.
 */
int trigger_source_read_andor_verify(mstr *trigname, trigger_action trigger_op)
{
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	gv_namehead		*save_gv_target;
	sgm_info		*save_sgm_info_ptr;
	int			src_fetch_status;
	sgmnt_addrs		*csa;	/* Used in SWITCH_TO_DEFAULT_REGION macro */
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];
	uint4			cycle;
	DEBUG_ONLY(unsigned int	lcl_t_tries;)
	enum cdb_sc		failure;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(NULL != trigname);
	assert((NULL != trigname->addr) && (0 != trigname->len));
	/* Before we try to save anything, see if there is something to save and initialize stuff if not */
	if (NULL == gd_header)
	{	/* If we do initialize things, start off in the default region since we need it shortly anyway */
		gvinit();
		SWITCH_TO_DEFAULT_REGION;
	}
	SAVE_TRIGGER_REGION_INFO;
	DBGTRIGR((stderr, "trigger_source_raov: Entered with trigger action %d\n", trigger_op));
	/* First determination is if a TP fence is already in operation or not */
	if (0 == dollar_tlevel)
	{	/* We need a TP fence - provide one */
		assert(!donot_INVOKE_MUMTSTART);
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = TRUE);
		/* 0 ==> save no locals but RESTART OK. Note we do NOT mark this as a TRIGGER_IMPLICIT_TSTART because we
		 * both have our own condition handler to take care of thrown restarts and because this is not a trigger
		 * execution thing - just a load and possibly a trigger compile.
		 */
		op_tstart(IMPLICIT_TSTART, TRUE, &literal_batch, 0);
		for (;;)
		{	/* Now that we are TP wrapped, fetch the trigger source lines from the ^#t global */
			DEBUG_ONLY(lcl_t_tries = t_tries);
			src_fetch_status = trigger_source_raov_tpwrap_helper(trigname, trigger_op);
			if ((0 == src_fetch_status) || (TRIG_FAILURE_RC == src_fetch_status))
			{
				assert(0 == dollar_tlevel); /* op_tcommit should have made sure of this */
				break;
			}
			/* A restart has been signalled inside trigger fetch code for this possibly implicit TP wrapped
			 * transaction. Redo source fetch logic.
			 */
			assert(ERR_TPRETRY == src_fetch_status);
			assert(CDB_STAGNATE >= t_tries);
			assert(0 < t_tries);
			assert((CDB_STAGNATE == t_tries) || (lcl_t_tries == t_tries - 1));
			failure = LAST_RESTART_CODE;
			assert(((cdb_sc_onln_rlbk1 != failure) && (cdb_sc_onln_rlbk2 != failure))
				|| !gv_target || !gv_target->root);
			assert((cdb_sc_onln_rlbk2 != failure) || TREF(dollar_zonlnrlbk));
			if (cdb_sc_onln_rlbk2 == failure)
				rts_error(VARLSTCNT(1) ERR_DBROLLEDBACK);
			/* else if (cdb_sc_onln_rlbk1 == status) we don't need to do anything other than proceeding with the next
			 * retry. Even though online rollback restart resets root block to zero for all gv_targets, ^#t root is
			 * always established in gvtr_db_read_hasht (called below). We don't care about the root block being reset
			 * for other gv_target because when they are referenced later in the process, op_gvname will be done and
			 * that will anyways establish the root block numbers once again.
			 */
		}
	} else
		/* no return if TP restart */
		src_fetch_status = trigger_source_raov(trigname, trigger_op);
	assert((0 == src_fetch_status) || (TRIG_FAILURE_RC == src_fetch_status));
	RESTORE_TRIGGER_REGION_INFO;
	return src_fetch_status;
}

/* Now TP wrap and fetch the trigger source lines from the ^#t global */
STATICFNDEF int trigger_source_raov_tpwrap_helper(mstr *trigname, trigger_action trigger_op)
{
	enum cdb_sc	cdb_status;
	int		rc;

	DBGTRIGR((stderr, "trigger_source_tpwrap_helper: Entered\n"));
	ESTABLISH_RET(trigger_source_raov_ch, SIGNAL);
	assert(donot_INVOKE_MUMTSTART);
	rc = trigger_source_raov(trigname, trigger_op);
	assert((0 == rc) || (TRIG_FAILURE_RC == rc));
	/* Finish it now verifying it completed successfully */
	GVTR_OP_TCOMMIT(cdb_status);
	if (cdb_sc_normal != cdb_status)
	{
		DBGTRIGR((stderr, "trigger_source_tpwrap_helper: Commit failed - throwing TP restart\n"));
		t_retry(cdb_status);
	}
	REVERT;
	return rc;
}

/* Routine to do the dirty work of resolving a trigger name into a trigger and perform the missing parts of
 * loading the trigger, loading the source, verifying proper source/trigger is loaded and compiling if
 * desired. If we complete successfully, returns 0. Error returns caught by condition handlers can return other values.
 */
STATICFNDEF int trigger_source_raov(mstr *trigname, trigger_action trigger_op)
{
	mname_entry		gvent;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	rhdtyp			*rtn_vector;
	gv_namehead		*gvt, *hasht_tree;
	gvt_trigger_t		*gvt_trigger;
	mstr			gbl, xecute_buff;
	int			index;
	mval			trig_index;
	gv_trigger_t		*trigdsc;
	uint4			cycle_start;
	boolean_t		triggers_reloaded, db_trigger_cycle_mismatch, ztrig_cycle_mismatch;

	assert(dollar_tlevel);		/* A TP wrap should have been done by the caller if needed */
	/* First lets locate the trigger. Try simple way first - lookup in routine name table */
	if (NULL == (rtn_vector = find_rtn_hdr(trigname)))	/* Note assignment */
	{	/* Wasn't found - look for it the harder way in the #t of the default region */
		if(TRIG_FAILURE == trigger_source_raov_trigload(trigname, &trigdsc))
			return TRIG_FAILURE_RC;
	} else
	{	/* Have a routine header addr. From that we can get the gv_trigger_t descriptor and from that, the
		 * gvt_trigger and other necessaries
		 */
		trigdsc = (gv_trigger_t *)rtn_vector->trigr_handle;
		gvt_trigger = trigdsc->gvt_trigger;			/* We now know our base block now */
		index = trigdsc - gvt_trigger->gv_trig_array + 1;	/* We now know our trigger index value */
		gvt = gv_target = gvt_trigger->gv_target;		/* gv_target contains global name */
		gbl.addr = gvt->gvname.var_name.addr;
		gbl.len = gvt->gvname.var_name.len;
		TP_CHANGE_REG_IF_NEEDED(gvt->gd_csa->region);
		csa = cs_addrs;
		csd = csa->hdr;
		assert(csd == cs_data);
		/* Verify trigger is current. Note we use CSA for this check since within this transaction we could have multiple
		 * triggers from the same global in flight preventing us from reloading a trigger. By checking CSA, we at least get
		 * a consistent trigger view and depend on CSA being checked as current in op_tcommit.
		 */
		triggers_reloaded = FALSE;
		SHM_READ_MEMORY_BARRIER;
		tp_set_sgm();
		db_trigger_cycle_mismatch = (csa->db_trigger_cycle != gvt->db_trigger_cycle);
		ztrig_cycle_mismatch = (csa->db_dztrigger_cycle && (gvt->db_dztrigger_cycle != csa->db_dztrigger_cycle));
		if (db_trigger_cycle_mismatch || ztrig_cycle_mismatch)
		{       /* The process' view of the triggers is stale. We cannot proceed unless the triggers get reloaded.
			 * If triggers have been driven for this global in this transaction, we have to throw a restart. To
			 * reload and go if triggers have already been driven creates a potential consistency issues plus
			 * the possibility that we could remove a trigger actively running which will cause major issues
			 * when the trigger returns.
			 *
			 * To prevent these sort of issues, we compare the local_tn value when the last trigger was driven
			 * in this global (recorded by gtm_trigger() in gvt->trig_local_tn) to the current local_tn value.
			 * If the same, we have to restart. Else, we can reload the triggers and keep going.
			 *
			 * Triggers can be invoked only by GT.M and Update process. Out of these, we expect only
			 * GT.M to see restarts due to concurrent trigger changes. Update process is the only
			 * updater on the secondary so we dont expect it to see any concurrent trigger changes
			 * Assert accordingly. Note similar asserts occur in t_end.c and tp_tend.c.
			 */
			DBGTRIGR((stderr, "trigger_source_raov: Trigger cycle difference detected - db_trigger_cycle - "
				  "csa: %d, csd: %d, gvt: %d  db_ztrigger_cycle: csa: %d, gvt: %d\n",
				  csa->db_trigger_cycle, csd->db_trigger_cycle, gvt->db_trigger_cycle,
				  csa->db_dztrigger_cycle, gvt->db_dztrigger_cycle));
			assert(IS_GTM_IMAGE);
			if ((local_tn == gvt->trig_local_tn) && db_trigger_cycle_mismatch)
			{	/* Already dispatched trigger for this gvn in this transaction - must restart. But do so ONLY
				 * if the process' trigger view changed because of a concurrent trigger load/unload and NOT
				 * because of $ZTRIGGER as part of this transaction as that could cause unintended restarts.
				 */
				assert(CDB_STAGNATE > t_tries);
				DBGTRIGR((stderr, "trigger_source_raov: throwing TP restart\n"));
				t_retry(cdb_sc_triggermod);
			}
			cycle_start = csa->db_trigger_cycle;
			gvtr_db_read_hasht(csa);
			gvt_trigger = gvt->gvt_trigger;
			if (NULL == gvt_trigger)
			{	/* No triggers were loaded for this region (all gone now) */
				CLEAR_IMPLICIT_TP_BEFORE_ERROR;
				rts_error(VARLSTCNT(4) ERR_TRIGNAMENF, 2, trigname->len, trigname->addr);
			}
			gvt->db_trigger_cycle = cycle_start;
			gvt->db_dztrigger_cycle = csa->db_dztrigger_cycle;
			DBGTRIGR((stderr, "trigger_source_raov: triggers reloaded - "
				  "gvt->db_trigger_cycle updated to %d\n", gvt->db_trigger_cycle));
			if (TRIG_FAILURE == trigger_source_raov_trigload(trigname, &trigdsc))
				return TRIG_FAILURE_RC;
			triggers_reloaded = TRUE;
		} else
			DBGTRIGR((stderr, "trigger_source_raov: trigger validated\n"));
		/* Only proceed with this next section at this point if triggers have not been reloaded. If they have
		 * been reloaded, the rtn_vector address will have changed causing issues in this section. In that case,
		 * we just need to fall out of this section to the common section which rebuilds things as necessary.
		 */
		if (!triggers_reloaded)
		{	/* Triggers were not reloaded - see if we need to load the source or not */
			if (TRIGGER_COMPILE == trigger_op)
			{	/* This trigger has been verified so if it is already compiled, we are done */
				if (NULL != trigdsc->rtn_desc.rt_adr)
					return 0;
			}
			/* Else we need the trigger source loaded */
			if (0 == ((gv_trigger_t *)rtn_vector->trigr_handle)->xecute_str.str.len)
			{
				SETUP_TRIGGER_GLOBAL;
				INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
				assert(0 == trigdsc->xecute_str.str.len);	/* Make sure not replacing/losing a buffer */
				i2mval(&trig_index, index);
				xecute_buff.addr = trigger_gbl_fill_xecute_buffer(gbl.addr, gbl.len, &trig_index, NULL,
										  (int4 *)&xecute_buff.len);
				trigdsc->xecute_str.str = xecute_buff;
			}
		}
		/* We have referenced this trigger's source. Mark it in gv_target so we know if we have to restart later
		 * if trigger changes instead of just reloading it on-the-fly.
		 */
		gvt->trig_local_tn = local_tn;
	}
	/* If the trigger is not already compiled, it needs to be since the routine header is the method for obtaining the
	 * trigger descriptor. If routine is already compiled, we don't need to compile it again.
	 */
	if ((TRIGGER_COMPILE == trigger_op) || (NULL == trigdsc->rtn_desc.rt_adr))
	{
		if (0 != gtm_trigger_complink(trigdsc, TRUE))
		{
			PRN_ERROR;	/* Flush out any compiler messages for compile record */
			rts_error(VARLSTCNT(4) ERR_TRIGCOMPFAIL, 2, trigdsc->rtn_desc.rt_name.len, trigdsc->rtn_desc.rt_name.addr);
		}
		assert(trigdsc->rtn_desc.rt_adr);
		assert(trigdsc->rtn_desc.rt_adr == CURRENT_RHEAD_ADR(trigdsc->rtn_desc.rt_adr));
		/* If compile only, the source code is no longer needed so release it */
		if ((TRIGGER_COMPILE == trigger_op) && (0 < trigdsc->xecute_str.str.len))
		{
			free(trigdsc->xecute_str.str.addr);
			trigdsc->xecute_str.str.addr = NULL;
			trigdsc->xecute_str.str.len = 0;
		}
	} else
	{
		assert(TRIGGER_SRC_LOAD == trigger_op);
		assert(NULL != trigdsc->xecute_str.str.addr);
		assert(0 < trigdsc->xecute_str.str.len);
	}
	return 0;
}

/* Routine called when need triggers loaded for a given global */
STATICFNDEF boolean_t trigger_source_raov_trigload(mstr *trigname, gv_trigger_t **ret_trigdsc)
{
	mname_entry		gvent;
	mval			val;
	char			*ptr;
	int			len;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	gv_namehead		*gvt, *hasht_tree;
	gvt_trigger_t		*gvt_trigger;
	mstr			gbl, xecute_buff;
	int			index;
	mval			trig_index;
	gv_trigger_t		*trigdsc;
	uint4			cycle_start;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Find region trigger name is in */
	if (!trigger_read_name_entry(trigname, &val))
	{	/* Trigger name not found - nothing we can do */
		if (!TREF(in_op_fntext))
		{
			CLEAR_IMPLICIT_TP_BEFORE_ERROR;
			rts_error(VARLSTCNT(4) ERR_TRIGNAMENF, 2, trigname->len, trigname->addr);
		}
		return TRIG_FAILURE;
	}
	/* Extract region name and trigger index number from result */
	ptr = val.str.addr;
	len = STRLEN(ptr);	/* Do it this way since "val" has multiple fields null separated */
	ptr += len;
	assert(('\0' == *ptr) && (val.str.len > len));
	ptr++;
	A2I(ptr, val.str.addr + val.str.len, index);
	gbl.addr = val.str.addr;
	gbl.len = len;
	GV_BIND_NAME_ONLY(gd_header, &gbl);	/* does tp_set_sgm() */
	gvt = gv_target;
	assert(cs_addrs == gvt->gd_csa);
	csa = gvt->gd_csa;
	csd = csa->hdr;
	assert(cs_data == csd);
	/* We now know the global/region we need to load triggers for - go for it */
	cycle_start = csd->db_trigger_cycle;
	gvtr_db_read_hasht(csa);
	gvt_trigger = gvt->gvt_trigger;
	if (NULL == gvt_trigger)
	{	/* No trigger were loaded for this region (all gone now) */
		CLEAR_IMPLICIT_TP_BEFORE_ERROR;
		rts_error(VARLSTCNT(4) ERR_TRIGNAMENF, 2, trigname->len, trigname->addr);
	}
	gvt->db_trigger_cycle = cycle_start;
	gvt->db_dztrigger_cycle = csa->db_dztrigger_cycle;
	gvt->trig_local_tn = local_tn;		/* Mark this trigger as being referenced in this transaction */
	DBGTRIGR((stderr, "trigger_source_raov_trigload: gvt->db_trigger_cycle updated to %d\n",
		  gvt->db_trigger_cycle));
	SETUP_TRIGGER_GLOBAL;
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	trigdsc = &gvt_trigger->gv_trig_array[index - 1];
	assert(0 == trigdsc->xecute_str.str.len);	/* Make sure not replacing/losing a buffer */
	i2mval(&trig_index, index);
	xecute_buff.addr = trigger_gbl_fill_xecute_buffer(gbl.addr, gbl.len, &trig_index, NULL, (int4 *)&xecute_buff.len);
	trigdsc->xecute_str.str = xecute_buff;
	*ret_trigdsc = trigdsc;
	return TRIG_SUCCESS;
}

#endif /* GTM_TRIGGER */
