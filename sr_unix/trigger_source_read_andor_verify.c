/****************************************************************
 *								*
 * Copyright (c) 2011-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#ifdef GTM_TRIGGER
#include "gtmio.h"
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
#include "trigger_read_andor_locate.h"
#include "gtm_trigger_trc.h"
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include "hashtab.h"			/* for STR_HASH (in COMPUTE_HASH_MNAME) */
#include "targ_alloc.h"			/* for SET_GVTARGET_TO_HASHT_GBL */
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
#include "hashtab_mname.h"
#include "hashtab_str.h"		/* needed by trigger_update_protos.h */
#include "trigger_update_protos.h"	/* for trigger_name_search prototype */
#include "change_reg.h"			/* for "change_reg" prototype */
#include "gvnh_spanreg.h"
#include "min_max.h"
#include "io.h"
#include "repl_msg.h"			/* for gtmsource.h */
#include "gtmsource.h"			/* for jnlpool_addrs_ptr_t */

GBLREF	uint4			dollar_tlevel;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_addr			*gd_header;
GBLREF	gv_key			*gv_currkey;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	gv_namehead		*gv_target;
GBLREF	int			tprestart_state;
GBLREF	tp_frame		*tp_pointer;
GBLREF	int4			gtm_trigger_depth;
GBLREF	trans_num		local_tn;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	rtn_tabent		*rtn_names_end;
#ifdef DEBUG
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
#endif

LITREF	mval			literal_batch;


STATICFNDCL CONDITION_HANDLER(trigger_source_raov_ch);
STATICFNDCL int trigger_source_raov(mstr *trigname, gd_region *reg, rhdtyp **rtn_vec);
STATICFNDCL int trigger_source_raov_tpwrap_helper(mstr *trigname, gd_region *reg, rhdtyp **rtn_vec);
STATICFNDCL boolean_t trigger_source_raov_trigload(mstr *trigname, gv_trigger_t **ret_trigdsc, gd_region *reg);

error_def(ERR_DBROLLEDBACK);
error_def(ERR_TPRETRY);
error_def(ERR_TRIGCOMPFAIL);
error_def(ERR_TRIGNAMENF);

/* Similar condition handler to gvtr_tpwrap_ch except we don't insist on first_sgm_info being set */
CONDITION_HANDLER(trigger_source_raov_ch)
{
	int	rc;

	START_CH(TRUE);
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

gd_region *find_region(mstr *regname)
{
	gd_region	*reg, *reg_top;
	mstr		tmpstr;
	int		comp;

	assert(NULL != gd_header);
	for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions; reg < reg_top; reg++)
	{
		tmpstr.len = reg->rname_len;
		tmpstr.addr = (char *)reg->rname;
		MSTR_CMP(tmpstr, *regname, comp);
		if (0 == comp)
			return reg;
	}
	return NULL;
}

/* Routine to check on the named trigger. This routine is called from (at least) two places: the trigger_locate_andor_load()
 * where is it called when the attempted locating of a trigger failed so it must be loaded. The other time it is called from
 * op_setzbrk() to load and/or compile the trigger. In both cases, the trigger may be loaded or


 * its source loaded already so we perform a validation on it that it is the current trigger. If the trigger contents are
 * stale, Our actions depend on what mode we are in. If already in TP, we cause the trigger to be unloaded and signal a
 * restart. If not in TP, we just unload the trigger and work our full mojo on it to reload the current version.
 *
 * The following sitations can exist:
 *
 *   1. No trigger by the given name is loaded. For this situation, we need to locate and load the trigger and its source.
 *   2. Trigger is loaded but no source is in the trigger source buffer. For this situation, load the source and mark the
 *      trigger as part of the transaction.
 *   3. Trigger and source both loaded. For this situation, mark the trigger as part of the transaction.
 *
 * If a TP fence is not in place, we provide an implcit wrapper that will catch our restarts and reinvoke the logic
 *	that will reload the trigger from scratch. We do not verify that the trigger information in memory is fresh.
 *
 * If a TP FENCE is in place and we are in the final retry, we verify that the triggers are current and reload them if
 *      not. This avoids the possibility of using stale triggers.
 *
 * Note the trigger_locate_andor_load() routine acts much like a stripped down verison of this routine and its subroutines
 * so changes here should be reflected there.
 *
 * Note, this routine is for loading trigger source when we are not driving triggers. The trigger_fill_xecute_buffer()
 * should be used when fetching source for trigger execution because it is lighter weight with built-in trigger refetch
 * logic since we are using the globals the triggers live in. In this case, the trigger access is adhoc for the $TEXT()
 * ZPRINT and ZBREAK uses.
 */
int trigger_source_read_andor_verify(mstr *trigname, rhdtyp **rtn_vec)
{
	char			*ptr, *ptr_beg, *ptr_top;
	enum cdb_sc		failure;
	gd_region		*reg;
	int			src_fetch_status;
	mstr			regname;
	DEBUG_ONLY(unsigned int	lcl_t_tries;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(NULL != trigname);
	assert((NULL != trigname->addr) && (0 != trigname->len));
	if (NULL == gd_header)
		gvinit();
	DBGTRIGR((stderr, "trigger_source_read_andor_verify: Entered with $tlevel=%d, $trigdepth=%d\n",
		  dollar_tlevel, gtm_trigger_depth));
	/*
	 * Input parameter "trigname" is of the form
	 *	a) <21-BYTE-MAX-TRUNCATED-GBLNAME>#<AUTO-GENERATED-CNT>#[RUNTIME-DISAMBIGUATOR][/REGION-NAME] OR
	 *	b) <28-BYTE-USER-SPECIFIED-TRIGNAME>#[RUNTIME-DISAMBIGUATOR][/REGION-NAME]
	 * where
	 *	<21-BYTE-MAX-TRUNCATED-GBLNAME>#<AUTO-GENERATED-CNT> OR <28-BYTE-USER-SPECIFIED-TRIGNAME> is the
	 *		auto-generated or user-specified trigger name we are searching for
	 *	RUNTIME-DISAMBIGUATOR is the unique string appended at the end by the runtime to distinguish
	 *		multiple triggers in different regions with the same auto-generated or user-given name
	 *	REGION-NAME is the name of the region in the gld where we specifically want to search for trigger names
	 *	[] implies optional parts
	 *
	 * Example usages are
	 *	  x#         : trigger routine user-named "x"
	 *	  x#1#       : trigger routine auto-named "x#1"
	 *	  x#1#A      : trigger routine auto-named "x#1" but also runtime disambiguated by "#A" at the end
	 *	  x#/BREG    : trigger routine user-named "x" in region BREG
	 *	  x#A/BREG   : trigger routine user-named "x", runtime disambiguated by "#A", AND in region BREG
	 *	  x#1#/BREG  : trigger routine auto-named "x#1" in region BREG
	 *	  x#1#A/BREG : trigger routine auto-named "x#1", runtime disambiguated by "#A", AND in region BREG
	 */
	/* First lets locate the trigger. Try simple way first - lookup in routine name table.
	 * But "find_rtn_tabent" function has no clue about REGION-NAME so remove /REGION-NAME (if any) before invoking it.
	 */
	regname.len = 0;
	reg = NULL;
	for (ptr_beg = trigname->addr, ptr_top = ptr_beg + trigname->len, ptr = ptr_top - 1; ptr >= ptr_beg; ptr--)
	{
		/* If we see a '#' and have not yet seen a '/' we are sure no region-name disambiguator has been specified */
		if ('#' == *ptr)
			break;
		if ('/' == *ptr)
		{
			trigname->len = ptr - trigname->addr;
			ptr++;
			regname.addr = ptr;
			regname.len = ptr_top - ptr;
			reg = find_region(&regname);	/* find region "regname" in "gd_header" */
			if (NULL == reg)
			{	/* Specified region-name is not present in current gbldir.
	 			 * Treat non-existent region name as if trigger was not found.
				 */
				ISSUE_TRIGNAMENF_ERROR_IF_APPROPRIATE(trigname);
				return TRIG_FAILURE_RC;
			}
			break;
		}
	}
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
			src_fetch_status = trigger_source_raov_tpwrap_helper(trigname, reg, rtn_vec);
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
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DBROLLEDBACK);
			/* else if (cdb_sc_onln_rlbk1 == status) we don't need to do anything other than proceeding with the next
			 * retry. Even though online rollback restart resets root block to zero for all gv_targets, ^#t root is
			 * always established in gvtr_db_read_hasht (called below). We don't care about the root block being reset
			 * for other gv_target because when they are referenced later in the process, op_gvname will be done and
			 * that will anyways establish the root block numbers once again.
			 */
		}
	} else
		/* no return if TP restart */
		src_fetch_status = trigger_source_raov(trigname, reg, rtn_vec);
	assert((0 == src_fetch_status) || (TRIG_FAILURE_RC == src_fetch_status));
	DBGTRIGR((stderr, "trigger_source_read_andor_verify: leaving with source from 0x%lx\n",
		  (*rtn_vec) ? (*((rhdtyp **)rtn_vec))->trigr_handle : NULL));
	assert((0 != src_fetch_status) || (NULL != *rtn_vec));	/* Either got an error or rtnvector returned */
	return src_fetch_status;
}

/* Now TP wrap and fetch the trigger source lines from the ^#t global */
STATICFNDEF int trigger_source_raov_tpwrap_helper(mstr *trigname, gd_region *reg, rhdtyp **rtn_vec)
{
	enum cdb_sc	cdb_status;
	int		rc;

	DBGTRIGR((stderr, "trigger_source_tpwrap_helper: Entered\n"));
	ESTABLISH_RET(trigger_source_raov_ch, SIGNAL);
	assert(donot_INVOKE_MUMTSTART);
	rc = trigger_source_raov(trigname, reg, rtn_vec);
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
STATICFNDEF int trigger_source_raov(mstr *trigname, gd_region *reg, rhdtyp **rtn_vec)
{
	boolean_t		runtime_disambiguator_specified;
	gd_region		*save_gv_cur_region;
	gv_key			save_currkey[DBKEYALLOC(MAX_KEY_SZ)];
	gv_namehead		*gvt;
	gv_namehead		*save_gv_target;
	gvnh_reg_t		*gvnh_reg;
	gv_trigger_t		*trigdsc;
	gvt_trigger_t		*gvt_trigger;
	int			index;
	mident			rtn_name;
	mstr			gbl, xecute_buff;
	mval			trig_index;
	rhdtyp			*rtn_vector;
	rtn_tabent		*rttabent;
	sgm_info		*save_sgm_info_ptr;
	jnlpool_addrs_ptr_t	save_jnlpool;
	sgmnt_addrs		*csa, *regcsa;
	sgmnt_data_ptr_t	csd;
	boolean_t		db_trigger_cycle_mismatch, ztrig_cycle_mismatch, needs_reload = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(dollar_tlevel);		/* A TP wrap should have been done by the caller if needed */
	DBGTRIGR((stderr, "trigger_source_raov: Entered\n"));
	/* Before we try to save anything, see if there is something to save and initialize stuff if not */
	SAVE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
	if (NULL != *rtn_vec)
		rtn_vector = *rtn_vec;
	else if (find_rtn_tabent(&rttabent, trigname))
		rtn_vector = rttabent->rt_adr;
	else
		rtn_vector = NULL;
	DBGTRIGR((stderr, "trigger_source_raov: routine was %sfound\n", (NULL == rtn_vector)?"not ":""));
	/* If region is specified, a null-runtime-disambiguator is treated as if runtime-disambiguator was not specified.
	 * If region is NOT specified, a null-runtime-disambiguator is treated as if runtime-disambiguator was specified.
	 */
	runtime_disambiguator_specified = ('#' != trigname->addr[trigname->len - 1]);
	if (!runtime_disambiguator_specified && (NULL != reg))
	{	/* Region-name has been specified and no runtime-disambiguator specified. Need to further refine the
		 * search done by find_rtn_tabent to focus on the desired region in case multiple routines with the same
		 * trigger name (but different runtime-disambiguators) exist.
		 */
		rtn_name.len = MIN(trigname->len, MAX_MIDENT_LEN);
		rtn_name.addr = trigname->addr;
		if (!reg->open)
			gv_init_reg(reg, NULL);	/* Open the region before obtaining "csa" */
		regcsa = &FILE_INFO(reg)->s_addrs;
		assert('#' == rtn_name.addr[rtn_name.len - 1]);
		for ( ; rttabent <= rtn_names_end; rttabent++)
		{
			if ((rttabent->rt_name.len < rtn_name.len) || memcmp(rttabent->rt_name.addr, rtn_name.addr, rtn_name.len))
			{	/* Past the list of routines with same name as trigger but different runtime disambiguators */
				rtn_vector = NULL;
				break;
			}
			rtn_vector = rttabent->rt_adr;
			trigdsc = (gv_trigger_t *)rtn_vector->trigr_handle;
			gvt_trigger = trigdsc->gvt_trigger;
			gvt = gvt_trigger->gv_target;
			/* Target region and trigger routine's region do not match, continue */
			if (gvt->gd_csa != regcsa)
				continue;
			/* Check if global name associated with the trigger is indeed mapped to the corresponding region
			 * by the gld.  If not treat this case as if the trigger is invisible and move on
			 */
			gbl.addr = gvt->gvname.var_name.addr;
			gbl.len = gvt->gvname.var_name.len;
			TP_CHANGE_REG_IF_NEEDED(gvt->gd_csa->region);
			csa = cs_addrs;
			csd = csa->hdr;
			COMPUTE_HASH_MNAME(&gvt->gvname);
			GV_BIND_NAME_ONLY(gd_header, &gvt->gvname, gvnh_reg);	/* does tp_set_sgm() */
			if (((NULL == gvnh_reg->gvspan) && (gv_cur_region != reg))
			    || ((NULL != gvnh_reg->gvspan) && !gvnh_spanreg_ismapped(gvnh_reg, gd_header, reg)))
				continue;
			/* Target region and trigger routine's region match, break (this check is a formality) */
			if (gvt->gd_csa == regcsa)
				break;
		}
	}
	csa = NULL;
	if (NULL == rtn_vector)
	{	/* If runtime disambiguator was specified and routine is not found, look no further.
		 * Otherwise, look for it in the #t global of any (or specified) region in current gbldir.
		 */
		DBGTRIGR((stderr, "trigger_source_raov: find trigger by name without disambiguator\n"));
		if (runtime_disambiguator_specified
			|| (TRIG_FAILURE == trigger_source_raov_trigload(trigname, &trigdsc, reg)))
		{
			RESTORE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
			ISSUE_TRIGNAMENF_ERROR_IF_APPROPRIATE(trigname);
			return TRIG_FAILURE_RC;
		}
		rtn_vector = trigdsc->rtn_desc.rt_adr;
	} else
	{	/* Have a routine header addr. From that we can get the gv_trigger_t descriptor and from that, the
		 * gvt_trigger and other necessities.
		 */
		DBGTRIGR((stderr, "trigger_source_raov: routine header found, now load it\n"));
		trigdsc = (gv_trigger_t *)rtn_vector->trigr_handle;
		gvt_trigger = trigdsc->gvt_trigger;			/* We now know our base block now */
		index = trigdsc - gvt_trigger->gv_trig_array + 1;	/* We now know our trigger index value */
		gvt = gv_target = gvt_trigger->gv_target;		/* gv_target contains global name */
		gbl.addr = gvt->gvname.var_name.addr;
		gbl.len = gvt->gvname.var_name.len;
		TP_CHANGE_REG_IF_NEEDED(gvt->gd_csa->region);
		csa = cs_addrs;
		csd = csa->hdr;
		if (runtime_disambiguator_specified && (NULL != reg))
		{	/* Runtime-disambiguator has been specified and routine was found. But region-name-disambiguator
			 * has also been specified. Check if found routine is indeed in the specified region. If not
			 * treat it as a failure to find the trigger.
			 */
			if (!reg->open)
				gv_init_reg(reg, NULL);
			if (&FILE_INFO(reg)->s_addrs != csa)
			{
				RESTORE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr,
							save_jnlpool);
				ISSUE_TRIGNAMENF_ERROR_IF_APPROPRIATE(trigname);
				return TRIG_FAILURE_RC;
			}
			/* Check if global name is indeed mapped to this region by the gld.  If not treat this case as
			 * if the trigger is invisible and issue an error
			 */
			COMPUTE_HASH_MNAME(&gvt->gvname);
			GV_BIND_NAME_ONLY(gd_header, &gvt->gvname, gvnh_reg);	/* does tp_set_sgm() */
			if (((NULL == gvnh_reg->gvspan) && (gv_cur_region != reg))
			    || ((NULL != gvnh_reg->gvspan) && !gvnh_spanreg_ismapped(gvnh_reg, gd_header, reg)))
			{
				RESTORE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr,
							save_jnlpool);
				ISSUE_TRIGNAMENF_ERROR_IF_APPROPRIATE(trigname);
				return TRIG_FAILURE_RC;
			}
		}
		assert(csd == cs_data);
		tp_set_sgm();
		/* Ensure that we don't use stale triggers if we are in the third retry OR an explicit transaction */
		if ((CDB_STAGNATE <= t_tries) || (!tp_pointer->implicit_tstart))
		{
			ztrig_cycle_mismatch = (csa->db_dztrigger_cycle && (gvt->db_dztrigger_cycle != csa->db_dztrigger_cycle));
			db_trigger_cycle_mismatch = (csa->db_trigger_cycle != gvt->db_trigger_cycle);
			needs_reload = (db_trigger_cycle_mismatch || ztrig_cycle_mismatch);
			DBGTRIGR((stderr, "trigger_source_raov: ztrig_cycle_mismatch=%d\tdb_trigger_cycle_mismatch=%d\treload?%d\n",
				  ztrig_cycle_mismatch, db_trigger_cycle_mismatch, needs_reload));
			if (needs_reload && (TRIG_FAILURE == trigger_source_raov_trigload(trigname, &trigdsc, reg))
					&& (NULL == rtn_vector->source_code))
			{
				/* A reload failed (deleted or ^#t busted) and there is nothing cached, issue an error */
				RESTORE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr,
							save_jnlpool);
				ISSUE_TRIGNAMENF_ERROR_IF_APPROPRIATE(trigname);
				return TRIG_FAILURE_RC;
			}
		}
		/* Now that this TP has relied on this process' current trigger view, ensure that any later action in the same
		 * TP that detects and reloads newer triggers (e.g. trigger invocation) restarts the entire TP transaction.
		 */
		gvt->trig_local_tn = local_tn;
		/* If this trigger is already compiled, we are done since they are compiled with embed_source */
		if (NULL != trigdsc->rtn_desc.rt_adr)
		{
			DBGTRIGR((stderr, "trigger_source_raov: trigger already compiled, all done\n"));
			RESTORE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
			*rtn_vec = rtn_vector;
			return 0;
		}
		/* Else load the trigger source as needed. If needs_reload is true then the source was loaded above */
		if ((NULL == rtn_vector->source_code) && (!needs_reload))
		{
			DBGTRIGR((stderr, "trigger_source_raov: get the source\n"));
			SET_GVTARGET_TO_HASHT_GBL(csa);
			INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
			assert(0 == trigdsc->xecute_str.str.len);	/* Make sure not replacing/losing a buffer */
			i2mval(&trig_index, index);
			xecute_buff.addr = trigger_gbl_fill_xecute_buffer(gbl.addr, gbl.len, &trig_index, NULL,
									  (int4 *)&xecute_buff.len);
			trigdsc->xecute_str.str = xecute_buff;
		}
	}
	/* If the trigger is not already compiled, it needs to be since the routine header is the method for obtaining the
	 * trigger descriptor. If routine is already compiled, we don't need to compile it again.
	 */
	if (NULL == trigdsc->rtn_desc.rt_adr)
	{
		DBGTRIGR((stderr, "trigger_source_raov: compile it\n"));
		if (0 != gtm_trigger_complink(trigdsc, TRUE))
		{
			PRN_ERROR;	/* Flush out any compiler messages for compile record */
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_TRIGCOMPFAIL, 2,
				      trigdsc->rtn_desc.rt_name.len - 1, trigdsc->rtn_desc.rt_name.addr);
		}
		assert(trigdsc->rtn_desc.rt_adr);
		assert(trigdsc->rtn_desc.rt_adr == CURRENT_RHEAD_ADR(trigdsc->rtn_desc.rt_adr));
		rtn_vector = trigdsc->rtn_desc.rt_adr;
	} else
	{
		assert(rtn_vector && (NULL !=rtn_vector->source_code));
	}
	RESTORE_REGION_INFO(save_currkey, save_gv_target, save_gv_cur_region, save_sgm_info_ptr, save_jnlpool);
	assert(rtn_vector);
	assert(trigdsc == rtn_vector->trigr_handle);
	*rtn_vec = rtn_vector;
	return 0;
}

/* Routine called when need triggers loaded for a given global */
STATICFNDEF boolean_t trigger_source_raov_trigload(mstr *trigname, gv_trigger_t **ret_trigdsc, gd_region *reg)
{
	mval			*val;
	char			*ptr;
	int			len;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	gv_namehead		*gvt;
	gvt_trigger_t		*gvt_trigger;
	mstr			xecute_buff;
	mname_entry		gvname;
	int			index;
	mval			trig_index;
	gv_trigger_t		*trigdsc;
	uint4			cycle_start;
	gvnh_reg_t		*gvnh_reg;
	boolean_t		name_not_found;
	int			trig_protected_mval_push_count;

	trig_protected_mval_push_count = 0;
	INCR_AND_PUSH_MV_STENT(val); /* Protect val from garbage collection */
	assert(dollar_tlevel);
	DBGTRIGR((stderr, "trigger_source_raov_trigload: entry for %s\n", trigname->addr));
	/* Find region trigger name is in. If "region-name" has been specified, find only in that region. */
	name_not_found = !trigger_name_search(trigname->addr, trigname->len, val, &reg);
	if (name_not_found)
		RETURN_AND_POP_MVALS(TRIG_FAILURE);	/* Trigger name not found - nothing we can do */
	/* Extract region name and trigger index number from result */
	assert(NULL != reg);
	ptr = val->str.addr;
	len = MIN(val->str.len, MAX_MIDENT_LEN);	/* Look for NULL within the MIN */
	STRNLEN(ptr, len, len);
	ptr += len;
	if ((val->str.len == len) || ('\0' != *ptr))
	{
		if (CDB_STAGNATE > t_tries)
			t_retry(cdb_sc_triggermod);
		/* Return an error instead of TRIGDEFBAD. The caller will throw the error */
		RETURN_AND_POP_MVALS(TRIG_FAILURE);
	}
	ptr++;
	A2I(ptr, val->str.addr + val->str.len, index);
	if (1 > index)
	{	/* Trigger indexes cannot be less than 1 */
		if (CDB_STAGNATE > t_tries)
			t_retry(cdb_sc_triggermod);
		/* Return an error instead of TRIGDEFBAD. The caller will throw the error */
		RETURN_AND_POP_MVALS(TRIG_FAILURE);
	}
	gvname.var_name.addr = val->str.addr;
	gvname.var_name.len = len;
	COMPUTE_HASH_MNAME(&gvname);
	GV_BIND_NAME_ONLY(gd_header, &gvname, gvnh_reg);	/* does tp_set_sgm() */
	if (NULL != gvnh_reg->gvspan)
	{
		assert(gvnh_spanreg_ismapped(gvnh_reg, gd_header, reg)); /* "trigger_name_search" would have ensured this */
		GV_BIND_SUBSREG(gd_header, reg, gvnh_reg);	/* sets gv_target/gv_cur_region/cs_addrs */
	} else
	{	/* gv_target/gv_cur_region/cs_addrs would have been set by GV_BIND_NAME_ONLY */
		assert(gv_cur_region == reg);
	}
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
	{
		if (CDB_STAGNATE > t_tries)
			t_retry(cdb_sc_triggermod);
		/* Return an error instead of TRIGDEFBAD. The caller will throw the error */
		RETURN_AND_POP_MVALS(TRIG_FAILURE);
	}
	gvt->db_trigger_cycle = cycle_start;
	gvt->db_dztrigger_cycle = csa->db_dztrigger_cycle;
	gvt->trig_local_tn = local_tn;		/* Mark this trigger as being referenced in this transaction */
	DBGTRIGR((stderr, "trigger_source_raov_trigload: CSA->db_dztrigger_cycle=%d\n", csa->db_dztrigger_cycle));
	DBGTRIGR((stderr, "trigger_source_raov_trigload: gvt->db_trigger_cycle updated to %d\n", gvt->db_trigger_cycle));
	SET_GVTARGET_TO_HASHT_GBL(csa);
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	trigdsc = &gvt_trigger->gv_trig_array[index - 1];
	assert(0 == trigdsc->xecute_str.str.len);	/* Make sure not replacing/losing a buffer */
	i2mval(&trig_index, index);
	xecute_buff.addr = trigger_gbl_fill_xecute_buffer(gvname.var_name.addr, gvname.var_name.len,
							  &trig_index, NULL, (int4 *)&xecute_buff.len);
	trigdsc->xecute_str.str = xecute_buff;
	*ret_trigdsc = trigdsc;
	RETURN_AND_POP_MVALS(TRIG_SUCCESS);
}

#endif /* GTM_TRIGGER */
