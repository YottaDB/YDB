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

#include <stddef.h>
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_inet.h"	/* Required for gtmsource.h */

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cdb_sc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "jnl.h"
#include "gdscc.h"
#include "copy.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "rc_oflow.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "mv_stent.h"
#ifdef GTM_TRIGGER
# include "gv_trigger.h"
# include "gtm_trigger.h"
# include "gv_trigger_protos.h"
# include "subscript.h"
# include "stringpool.h"
#endif
#include "tp_frame.h"
#include "tp_restart.h"

/* Include prototypes */
#include "t_write.h"
#include "t_write_root.h"
#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "t_create.h"
#include "gvcst_blk_build.h"
#include "gvcst_expand_key.h"
#include "gvcst_protos.h"	/* for gvcst_search,gvcst_search_blk,gvcst_put prototype */
#include "op.h"			/* for op_add & op_tstart prototype */
#include "format_targ_key.h"	/* for format_targ_key prototype */
#include "gvsub2str.h"		/* for gvsub2str prototype */
#include "tp_set_sgm.h"		/* for tp_set_sgm prototype */
#include "op_tcommit.h"		/* for op_tcommit prototype */
#include "have_crit.h"
#include "error.h"
#include "gtmimagename.h" /* for spanning nodes */
#ifdef UNIX
#include "preemptive_db_clnup.h"
#endif

#ifdef GTM_TRIGGER
LITREF	mval	literal_null;
LITREF	mval	literal_one;
LITREF	mval	literal_zero;
#endif
LITREF	mval	literal_batch;
LITREF	mstr	nsb_dummy;

/* Globals that will not change in value across nested trigger calls of gvcst_put OR even if they might change in value,
 * the change is such that they dont need save/restore logic surrounding the "gtm_trigger" call. Any new GBLREFs that are
 * added in this module need to be examined for interference between gvcst_put and nested trigger call and any save/restore
 * logic (if needed) should be appropriately added surrounding the "gtm_trigger" invocation.
 */
GBLREF	boolean_t		gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database (except for curr_tn++) */
GBLREF	boolean_t		horiz_growth;
GBLREF	boolean_t		in_gvcst_incr;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	gv_key			*gv_altkey;
GBLREF	gv_namehead		*reset_gv_target;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	int			gv_fillfactor;
GBLREF	int			rc_set_fragment;	/* Contains offset within data at which data fragment starts */
GBLREF	int4			gv_keysize;
GBLREF	int4			prev_first_off, prev_next_off;
GBLREF	uint4			update_trans;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	uint4			dollar_tlevel;
GBLREF	uint4			process_id;
GBLREF	uint4			update_array_size, cumul_update_array_size;	/* the current total size of the update array */
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	unsigned int		t_tries;
GBLREF	cw_set_element		cw_set[CDB_CW_SET_SIZE];/* create write set. */
GBLREF	boolean_t		skip_dbtriggers;	/* see gbldefs.c for description of this global */
GBLREF	stack_frame		*frame_pointer;
GBLREF	mv_stent		*mv_chain;
#ifdef GTM_TRIGGER
GBLREF	int			tprestart_state;
GBLREF	int4			gtm_trigger_depth;
GBLREF	int4			tstart_trigger_depth;
GBLREF	boolean_t		skip_INVOKE_RESTART;
GBLREF	boolean_t		ztwormhole_used;	/* TRUE if $ztwormhole was used by trigger code */
#endif
#ifdef DEBUG
GBLREF	boolean_t		skip_block_chain_tail_check;
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
#endif

/* Globals that could change in value across nested trigger calls of gvcst_put AND need to be saved/restored */
GBLREF	boolean_t		is_dollar_incr;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	mval			*post_incr_mval;
GBLREF	mval			increment_delta_mval;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
UNIX_ONLY(GBLREF	enum gtmImageTypes	image_type;)
UNIX_ONLY(GBLREF	boolean_t 		span_nodes_disallowed;)

error_def(ERR_DBROLLEDBACK);
error_def(ERR_GVINCRISOLATION);
error_def(ERR_GVIS);
error_def(ERR_GVPUTFAIL);
error_def(ERR_REC2BIG);
error_def(ERR_RSVDBYTE2HIGH);
error_def(ERR_TEXT);
error_def(ERR_TPRETRY);
error_def(ERR_UNIMPLOP);

/* Before issuing an error, add GVT to the list of known gvts in this TP transaction in case it is not already done.
 * This GVT addition is usually done by "tp_hist" but that function has most likely not yet been invoked in gvcst_put.
 * Doing this addition will ensure we remember to reset any non-zero clue in dir_tree as part of tp_clean_up when a TROLLBACK
 * or TRESTART (implicit or explicit) occurs. Not doing so could means if an ERR_REC2BIG happens here, control will
 * go to the error trap and if it does a TROLLBACK (which does a tp_clean_up) we would be left with a potentially out-of-date
 * clue of GVT which if used for later global references could result in db integ errors.
 */
#define	ENSURE_VALUE_WITHIN_MAX_REC_SIZE(value, GVT)										\
{																\
	if (dollar_tlevel)													\
		ADD_TO_GVT_TP_LIST(GVT, RESET_FIRST_TP_SRCH_STATUS_FALSE); /* note: macro updates read_local_tn if necessary */	\
	if (VMS_ONLY(gv_currkey->end + 1 + SIZEOF(rec_hdr) +) value.len > gv_cur_region->max_rec_size)				\
	{															\
		if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))					\
			end = &buff[MAX_ZWR_KEY_SZ - 1];									\
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(10) ERR_REC2BIG, 4, VMS_ONLY(gv_currkey->end + 1 + SIZEOF(rec_hdr) +)	\
			  value.len, (int4)gv_cur_region->max_rec_size,								\
			  REG_LEN_STR(gv_cur_region), ERR_GVIS, 2, end - buff, buff);						\
	}															\
}

/* See comment before ENSURE_VALUE_WITHIN_MAX_REC_SIZE macro definition for why the ADD_TO_GVT_TP_LIST call below is necessary */
#define	ISSUE_RSVDBYTE2HIGH_ERROR(GVT)												\
{																\
	if (dollar_tlevel)													\
		ADD_TO_GVT_TP_LIST(GVT, RESET_FIRST_TP_SRCH_STATUS_FALSE); /* note: macro updates read_local_tn if necessary */	\
	/* The record that is newly inserted/updated does not fit by itself in a separate block					\
	 * if the current reserved-bytes for this database is taken into account. Cannot go on.					\
	 */															\
	if (0 == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))						\
		end = &buff[MAX_ZWR_KEY_SZ - 1];										\
	rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(11) ERR_RSVDBYTE2HIGH, 5, new_blk_size_single,				\
		REG_LEN_STR(gv_cur_region), blk_size, blk_reserved_bytes,							\
		ERR_GVIS, 2, end - buff, buff);											\
}

#define	RESTORE_ZERO_GVT_ROOT_ON_RETRY(LCL_ROOT, GV_TARGET, DIR_HIST, DIR_TREE)					\
{														\
	if (!LCL_ROOT)												\
	{													\
		assert(NULL != DIR_HIST);									\
		assert(DIR_TREE == GV_TARGET->gd_csa->dir_tree);						\
		/* t_retry only resets gv_target->clue and not the clue of the directory tree.			\
		 * But DIR_HIST non-null implies the directory tree was used in a gvcst_search and hence	\
		 * was validated (in t_end/tp_hist),so we need to reset its clue before the next try.		\
		 */												\
		DIR_TREE->clue.end = 0;										\
		/* We had reset the root block from zero to a non-zero value within				\
		 * this function, but since we are restarting, we can no longer be				\
		 * sure of the validity of the root block. Reset it to 0 so it will				\
		 * be re-determined in the next global reference.						\
		 */												\
		GV_TARGET->root = 0;										\
	}													\
}

#define RECORD_FITS_IN_A_BLOCK(VAL, KEY, BLK_SZ, RESERVED_BYTES)						\
	((VAL)->str.len <= COMPUTE_CHUNK_SIZE(KEY, BLK_SZ, RESERVED_BYTES))

#define ZKILL_NODELETS												\
{														\
	APPEND_HIDDEN_SUB(gv_currkey);										\
	if (gv_target->root)											\
		gvcst_kill2(FALSE, NULL, TRUE); /* zkill any existing spanning nodelets.. */			\
	RESTORE_CURRKEY(gv_currkey, oldend);									\
}

#define POP_MVALS_FROM_M_STACK_IF_REALLY_NEEDED(lcl_span_status, ztold_mval, save_msp, save_mv_chain)		\
{														\
	if (!lcl_span_status)											\
		POP_MVALS_FROM_M_STACK_IF_NEEDED(ztold_mval, save_msp, save_mv_chain);				\
}

#ifdef DEBUG
# define DBG_SAVE_VAL_AT_FUN_ENTRY							\
{	/* Save copy of "val" at function entry.					\
	 * Make sure this is not touched by any nested trigger code */			\
	dbg_lcl_val = val;								\
	dbg_vallen = val->str.len;							\
	memcpy(dbg_valbuff, val->str.addr, MIN(ARRAYSIZE(dbg_valbuff), dbg_vallen));	\
}

# define DBG_CHECK_VAL_AT_FUN_EXIT										\
{	/* Check "val" is same as what it was at function entry.(i.e. was not touched by nested trigger code).	\
	 * The only exception is if $ZTVAL changed "val" in which case gvcst_put would have been redone. */	\
	assert(dbg_vallen == dbg_lcl_val->str.len);								\
	assert(0 == memcmp(dbg_valbuff, dbg_lcl_val->str.addr, MIN(ARRAYSIZE(dbg_valbuff), dbg_vallen)));	\
}
#else
# define DBG_SAVE_VAL_AT_FUN_ENTRY
# define DBG_CHECK_VAL_AT_FUN_EXIT
#endif

#define	GOTO_RETRY									\
{											\
	GTMTRIG_DBG_ONLY(dbg_trace_array[dbg_num_iters].retry_line = __LINE__);		\
	goto retry;									\
}

CONDITION_HANDLER(gvcst_put_ch)
{
	int rc;

	START_CH;
	if ((int)ERR_TPRETRY == SIGNAL)
	{	/* delay tp_restart till after the long jump and some other stuff */
		UNWIND(NULL, NULL);
	}
	NEXTCH;
}

void	gvcst_put(mval *val)
{
	boolean_t			sn_tpwrapped, fits;
	boolean_t			est_first_pass;
	mval				val_ctrl, val_piece, val_dummy;
	mval				*pre_incr_mval, *save_val;
	block_id			lcl_root;
	int				gblsize, chunk_size, i, oldend;
	unsigned short			numsubs;
	unsigned char			mychars[MAX_NSBCTRL_SZ];
	int				save_dollar_tlevel, rc;
	boolean_t			save_in_gvcst_incr; /* gvcst_put2 sets this FALSE, so save it in case we need to back out */
	span_parms			parms;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	parms.span_status = FALSE;
	parms.blk_reserved_bytes = cs_data->reserved_bytes; /* Only want to read once for consistency */
	parms.enable_trigger_read_and_fire = TRUE;
	parms.enable_jnl_format = TRUE;
	lcl_root = gv_target->root;
	VMS_ONLY(gvcst_put2(val, &parms));
#	ifdef UNIX /* deal with possibility of spanning nodes */
	DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
	fits = RECORD_FITS_IN_A_BLOCK(val, gv_currkey, cs_data->blk_size, parms.blk_reserved_bytes);
	save_in_gvcst_incr = in_gvcst_incr;
	if (fits)
	{
		gvcst_put2(val, &parms);
		if (!parms.span_status)
		{
			assert(save_dollar_tlevel == dollar_tlevel);
			return; /* We've successfully set a normal non-spanning global. */
		}
	}
	RTS_ERROR_IF_SN_DISALLOWED;
	/* Either we need to create a spanning node, or kill one before resetting it */
	GTMTRIG_ONLY(parms.ztold_mval = NULL);
	cs_data->span_node_absent = FALSE;
	oldend = gv_currkey->end;
	val_dummy.str = nsb_dummy;
	if (!dollar_tlevel)
	{
		sn_tpwrapped = TRUE;
		save_val = val;
		/* We pass the IMPLICIT_TRIGGER_TSTART flag because we might invoke a trigger, and in that case we want to avoid
		 * invoking a restart from within gtm_trigger. Since we remove gvcst_put_ch before invoking the trigger
		 * (see comment in errorsp.h), an attempt to invoke a restart returns us back to the mdb_condition_handler created
		 * by the initial dm-start. Unwinding from there returns to the OS -- i.e., the process silently dies.
		 * Also note that we need to ensure the retry logic at the bottom of gvcst_put2 is executed in case a restart
		 * happens within a trigger invocation. We want to return from gvtr_match_n_invoke so we can goto retry.
		 */
		op_tstart((IMPLICIT_TSTART + IMPLICIT_TRIGGER_TSTART), TRUE, &literal_batch, 0);
		frame_pointer->flags |= SFF_IMPLTSTART_CALLD;
		assert(!donot_INVOKE_MUMTSTART);
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = TRUE);
		ESTABLISH_NORET(gvcst_put_ch, est_first_pass);
		if (est_first_pass)
		{	/* did a long jump back to here */
			val = save_val;
			GTMTRIG_ONLY(POP_MVALS_FROM_M_STACK_IF_NEEDED(parms.ztold_mval,
					parms.save_msp, ((mv_stent *)parms.save_mv_chain)));
			preemptive_db_clnup(ERROR); /* Bluff about SEVERITY to reset gv_target and reset_gv_target tp_restart resets
						     * but not reset_gv_target This matches flow with non-spanning-node tp_restart,
						     * which does preemptive_db_clnup in mdb_condition_handler
						     */
			rc = tp_restart(1, !TP_RESTART_HANDLES_ERRORS);
			DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
			RESTORE_ZERO_GVT_ROOT_ON_RETRY(lcl_root, gv_target, &cs_addrs->dir_tree->hist, cs_addrs->dir_tree);
			fits = RECORD_FITS_IN_A_BLOCK(val, gv_currkey, cs_data->blk_size, parms.blk_reserved_bytes);
			if (cdb_sc_onln_rlbk2 == LAST_RESTART_CODE)
				/* Database was taken back to a different logical state. We are an implicit TP transaction, and
				 * as in the case of implicit TP for triggers, we issue a DBROLLEDBACK error that the application
				 * programmer can catch.
				 */
				rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DBROLLEDBACK);
		}
		tp_set_sgm();
		GVCST_ROOT_SEARCH;
	} else
		sn_tpwrapped = FALSE;
	parms.span_status = TRUE;
	parms.enable_trigger_read_and_fire = TRUE;
	parms.enable_jnl_format = TRUE;
	parms.ztval_gvcst_put_redo = FALSE;
	if (save_in_gvcst_incr)
	{
		in_gvcst_incr = FALSE;		/* allow gvcst_put2 to do a regular set */
		PUSH_MV_STENT(MVST_MVAL);       /* protect pre_incr_mval from stp_gcol */
		pre_incr_mval = &mv_chain->mv_st_cont.mvs_mval;
		gvcst_get(pre_incr_mval);	/* what if it doesn't exist? needs to be treated as 0 */
		pre_incr_mval->mvtype = MV_STR;
		op_add(pre_incr_mval, &increment_delta_mval, post_incr_mval);
		POP_MV_STENT();			/* pre_incr_mval */
		assert(MV_IS_NUMERIC(post_incr_mval));
		MV_FORCE_STR(post_incr_mval);
		val = post_incr_mval;		/* its a number, should fit in single block.. unless ridick rsrvdbytes.. */
		fits = RECORD_FITS_IN_A_BLOCK(val, gv_currkey, cs_data->blk_size, parms.blk_reserved_bytes);
	}
	/* Set the primary node. If we're setting a spanning node, that the primary node will have a dummy value, $char(0).
	 * Journal formatting and triggers happen here. If ztval changed during trigger invocations to a large value,
	 * we do a second gvcst_put2 to set the new value.
	 */
	parms.val_forjnl = val;
	gvcst_put2(((fits) ? val : &val_dummy), &parms);
	parms.enable_trigger_read_and_fire = FALSE;
#	ifdef GTM_TRIGGER
	if (parms.ztval_gvcst_put_redo)
	{
		val = parms.ztval_mval;
		parms.enable_jnl_format = TRUE;	/* new val needs to be journaled */
		parms.val_forjnl = val;
		fits = RECORD_FITS_IN_A_BLOCK(val, gv_currkey, cs_data->blk_size, parms.blk_reserved_bytes);
		assert(!fits);
		gvcst_put2(&val_dummy, &parms);
	}
#	endif
	ZKILL_NODELETS; /* Even if not creating a spanning node, may be replacing one */
	if (!fits)
	{	/* Need to create a spanning node. Break value up into chunks. */
		parms.enable_jnl_format = FALSE; /* jnl formatting already done */
		APPEND_HIDDEN_SUB(gv_currkey);
		chunk_size = COMPUTE_CHUNK_SIZE(gv_currkey, cs_data->blk_size, parms.blk_reserved_bytes);
		gblsize = val->str.len;
		numsubs = DIVIDE_ROUND_UP(gblsize, chunk_size);
		PUT_NSBCTRL(mychars, numsubs, gblsize);
		val_ctrl.str.addr = (char *)mychars;
		val_ctrl.str.len = 6;
		/* Count the spanning node set as one set */
		INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_set, 1);
		gvcst_put2(&val_ctrl, &parms);	/* Set control subscript, indicating glbsize and number of chunks */
		for (i = 0; i < numsubs; i++)
		{
			NEXT_HIDDEN_SUB(gv_currkey, i);
			val_piece.str.len = MIN(chunk_size, gblsize - i * chunk_size);
			val_piece.str.addr = val->str.addr + i * chunk_size;
			gvcst_put2(&val_piece, &parms);
		}
		RESTORE_CURRKEY(gv_currkey, oldend);
	}
	GTMTRIG_ONLY(POP_MVALS_FROM_M_STACK_IF_NEEDED(parms.ztold_mval, parms.save_msp, ((mv_stent *)parms.save_mv_chain)));
		/* pop any stacked mvals before op_tcommit as it does its own popping */
	if (sn_tpwrapped)
	{
		op_tcommit();
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = FALSE);
		REVERT; /* remove our condition handler */
	}
	assert(save_dollar_tlevel == dollar_tlevel);
#	endif /* ifdef UNIX */
}

void	gvcst_put2(mval *val, span_parms *parms)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	int4			blk_size, blk_fill_size, blk_reserved_bytes;
	const int4		zeroes = 0;
	boolean_t		jnl_format_done, is_dummy, needfmtjnl, fits, lcl_span_status;
	blk_segment		*bs1, *bs_ptr, *new_blk_bs;
	block_id		allocation_clue, tp_root, gvt_for_root, blk_num, last_split_blk_num[MAX_BT_DEPTH];
	block_index		left_hand_index, ins_chain_index, root_blk_cw_index, next_blk_index;
	block_offset		next_offset, first_offset, ins_off1, ins_off2, old_curr_chain_next_off;
	cw_set_element		*cse, *cse_new, *old_cse;
	gv_namehead		*save_targ, *split_targ, *dir_tree;
	enum cdb_sc		status;
	gv_key			*temp_key;
	int			tmp_cmpc;
	mstr			value;
	off_chain		chain1, curr_chain, prev_chain, chain2;
	rec_hdr_ptr_t		curr_rec_hdr, extra_rec_hdr, next_rec_hdr, new_star_hdr, rp, tmp_rp;
	srch_blk_status		*bh, *bq, *tp_srch_status;
	srch_hist		*dir_hist;
	int			cur_blk_size, blk_seg_cnt, delta, i, j, left_hand_offset, n, ins_chain_offset,
				new_blk_size_l, new_blk_size_r, new_blk_size_single, new_blk_size, blk_reserved_size,
				last_possible_left_offset, new_rec_size, next_rec_shrink, next_rec_shrink1, start_len,
				offset_sum, rec_cmpc, target_key_size, tp_lev, undo_index, cur_val_offset, curr_offset, bh_level;
	uint4			segment_update_array_size, key_top, cp2_len, bs1_2_len, bs1_3_len;
	char			*va, last_split_direction[MAX_BT_DEPTH];
	sm_uc_ptr_t		cp1, cp2, curr;
	unsigned short		extra_record_orig_size, rec_size, temp_short;
	unsigned int		prev_rec_offset, prev_rec_match, curr_rec_offset, curr_rec_match;
	boolean_t		copy_extra_record, level_0, new_rec, no_pointers, succeeded, key_exists;
	boolean_t		make_it_null, gbl_target_was_set, duplicate_set, new_rec_goes_to_right, need_extra_block_split;
	key_cum_value		*tempkv;
	jnl_format_buffer	*jfb, *ztworm_jfb;
	jnl_action		*ja;
	mval			*set_val;	/* actual right-hand-side value of the SET or $INCR command */
	mval			*val_forjnl;
	ht_ent_int4		*tabent;
	unsigned char		buff[MAX_ZWR_KEY_SZ], *end, old_ch, new_ch;
	sm_uc_ptr_t		buffaddr;
	block_id		lcl_root, last_split_bnum;
	sgm_info		*si;
	uint4			nodeflags;
	boolean_t		write_logical_jnlrecs, can_write_logical_jnlrecs, blk_match, is_split_dir_left;
	int			split_depth;
	mval			*ja_val;
	int			rc;
	int4			cse_first_off;
	enum split_dir		last_split_dir;
	int4			data_len;
#	ifdef GTM_TRIGGER
	boolean_t		is_tpwrap;
	boolean_t		ztval_gvcst_put_redo, skip_hasht_read;
	gtm_trigger_parms	trigparms;
	gvt_trigger_t		*gvt_trigger;
	gvtr_invoke_parms_t	gvtr_parms;
	int			gtm_trig_status;
	unsigned char		*save_msp;
	mv_stent		*save_mv_chain;
	mval			*ztold_mval = NULL;
	mval			*ztval_mval;
	mint			dlr_data;
	boolean_t		lcl_implicit_tstart;		/* local copy of the global variable "implicit_tstart" */
	mval			lcl_increment_delta_mval;	/* local copy of "increment_delta_mval" */
	boolean_t		lcl_is_dollar_incr;		/* local copy of is_dollar_incr taken at start of module.
								 * used to restore is_dollar_incr in case of TP restarts */
	mval			*lcl_post_incr_mval;		/* local copy of "post_incr_mval" at function entry.
								 * used to restore "post_incr_mval" in case of TP restarts */
	mval			*lcl_val;			/* local copy of "val" at function entry.
								 * used to restore "val" in case of TP restarts */
	mval			*lcl_val_forjnl;
	mval			*pval;				/* copy of "value" (an mstr), protected from stp gcol */
	DEBUG_ONLY(enum cdb_sc	save_cdb_status;)
#	endif
#	ifdef DEBUG
	char			dbg_valbuff[256];
	mstr_len_t		dbg_vallen;
	mval			*dbg_lcl_val;
	int			dbg_num_iters = -1;	/* number of iterations through gvcst_put */
	int			lcl_dollar_tlevel, lcl_t_tries;
	typedef struct
	{
		unsigned int	t_tries;
		int		retry_line;
		boolean_t	is_fresh_tn_start;
		boolean_t	is_dollar_incr;
		boolean_t	ztval_gvcst_put_redo;
		boolean_t	is_extra_block_split;
		mval		*val;
		boolean_t	lcl_implicit_tstart;
	} dbg_trace;
	/* We want to capture all pertinent information across each iteration of gvcst_put.
	 * There are 3 things that can contribute to a new iteration.
	 * 	a) restarts from the primary set.
	 * 		Max of 4 iterations.
	 * 	b) extra_block_split from the primary set. It can have its own set of restarts too.
	 * 		Max of 4 iterations per extra_block_split.
	 * 		The # of extra block splits could be arbitrary in case of non-TP but cannot be more than 1 for TP
	 * 		because in TP, we would have grabbed crit in the final retry and prevent any more concurrent updates.
	 * 	c) ztval_gvcst_put_redo. This in turn can have its own set of restarts and extra_block_split iterations.
	 * 		Could take a max of (a) + (b) = 4 + 4 = 8 iterations.
	 * 	Total of 16 max iterations. If ever a transaction goes for more than this # of iterations (theoretically
	 * 		possible in non-TP if a lot of extra block splits occur), we assert fail.
	 */
	dbg_trace		dbg_trace_array[16];
	boolean_t		is_fresh_tn_start;
	boolean_t		is_mm;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	is_dollar_incr = in_gvcst_incr;
	in_gvcst_incr = FALSE;
	csa = cs_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	assert(csd == cs_data);
	DEBUG_ONLY(is_mm = (dba_mm == csd->acc_meth);)
#	ifdef GTM_TRIGGER
	TRIG_CHECK_REPLSTATE_MATCHES_EXPLICIT_UPDATE(gv_cur_region, csa);
	if (IS_EXPLICIT_UPDATE)
	{	/* This is an explicit update. Set ztwormhole_used to FALSE. Note that we initialize this only at the
		 * beginning of the transaction and not at the beginning of each try/retry. If the application used
		 * $ztwormhole in any retsarting try of the transaction, we consider it necessary to write the
		 * TZTWORM/UZTWORM record even though it was not used in the succeeding/committing try.
		 */
		ztwormhole_used = FALSE;
	}
#	endif
	JNLPOOL_INIT_IF_NEEDED(csa, csd, cnl);
	blk_size = csd->blk_size;
	blk_reserved_bytes = parms->blk_reserved_bytes;
	blk_fill_size = (blk_size * gv_fillfactor) / 100 - blk_reserved_bytes;
	lcl_span_status = parms->span_status;
	if (lcl_span_status)
	{
		needfmtjnl = parms->enable_jnl_format;
		val_forjnl = parms->val_forjnl;
#		ifdef GTM_TRIGGER
		ztold_mval = parms->ztold_mval;
		skip_hasht_read = !parms->enable_trigger_read_and_fire;
#		endif
	} else
	{
		needfmtjnl = TRUE;
		val_forjnl = val;
	}
	jnl_format_done = !needfmtjnl; 	/* do "jnl_format" only once per logical non-tp transaction irrespective of
					 * number of retries or number of chunks if spanning node
					 */
	GTMTRIG_ONLY(
		ztval_gvcst_put_redo = FALSE;
		skip_hasht_read = FALSE;
	)
	assert(('\0' != gv_currkey->base[0]) && gv_currkey->end);
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	/* this needs to be initialized before any code that does a "goto retry" since this gets used there */
	save_targ = gv_target;
	gbl_target_was_set = (INVALID_GV_TARGET != reset_gv_target);
	if (INVALID_GV_TARGET != reset_gv_target)
		gbl_target_was_set = TRUE;
	else
	{
		gbl_target_was_set = FALSE;
		reset_gv_target = save_targ;
	}
	DBG_SAVE_VAL_AT_FUN_ENTRY;
	GTMTRIG_ONLY(
		lcl_implicit_tstart = FALSE;
		DEBUG_ONLY(gvtr_parms.num_triggers_invoked = -1;) /* set to an out-of-design value; checked by an assert */
	)
	DEBUG_ONLY(
		status = cdb_sc_normal;
		lcl_dollar_tlevel = dollar_tlevel;
	)
fresh_tn_start:
	DEBUG_ONLY(lcl_t_tries = -1;)
	DEBUG_ONLY(is_fresh_tn_start = TRUE;)
	assert(!jnl_format_done || (dollar_tlevel GTMTRIG_ONLY(&& ztval_gvcst_put_redo)) || lcl_span_status);
	T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_GVPUTFAIL);
tn_restart:
	/* t_tries should never decrease - it either increases or stays the same. If should decrease we could live-lock with
	 * an oscillating t_tries and never reach CDB_STAGNATE (go from optimistic to pessimistic concurrency). Since we
	 * typically do a normal increment and then, for certain conditions, do a complementary decrement, we assert that
	 * the net effect is never a decrease.
	 */
	assert(csa == cs_addrs);	/* no amount of retries should change cs_addrs from what it was at entry into gvcst_put */
	assert((((int)t_tries) > lcl_t_tries) || (CDB_STAGNATE == t_tries));
	DEBUG_ONLY(lcl_t_tries = t_tries;) /* update lcl_t_tries */
	DEBUG_ONLY(
		dbg_num_iters++;
		assert(dbg_num_iters < ARRAYSIZE(dbg_trace_array));
		dbg_trace_array[dbg_num_iters].is_fresh_tn_start = is_fresh_tn_start;
		dbg_trace_array[dbg_num_iters].t_tries = t_tries;
		is_fresh_tn_start = FALSE;
		dbg_trace_array[dbg_num_iters].is_dollar_incr = is_dollar_incr;
		GTMTRIG_ONLY(dbg_trace_array[dbg_num_iters].ztval_gvcst_put_redo = ztval_gvcst_put_redo;)
		dbg_trace_array[dbg_num_iters].val = val;
		GTMTRIG_ONLY(dbg_trace_array[dbg_num_iters].lcl_implicit_tstart = lcl_implicit_tstart;)
		dbg_trace_array[dbg_num_iters].is_extra_block_split = FALSE;
		dbg_trace_array[dbg_num_iters].retry_line = 0;
		split_targ = NULL;
	)
	assert(csd == cs_data);	/* To ensure they are the same even if MM extensions happened in between */
#	ifdef GTM_TRIGGER
	gvtr_parms.num_triggers_invoked = 0;	/* clear any leftover value */
	assert(!ztval_gvcst_put_redo || IS_PTR_INSIDE_M_STACK(val));
	is_tpwrap = FALSE;
	if (!skip_dbtriggers && !skip_hasht_read && parms->enable_trigger_read_and_fire)
	{
		GVTR_INIT_AND_TPWRAP_IF_NEEDED(csa, csd, gv_target, gvt_trigger, lcl_implicit_tstart, is_tpwrap, ERR_GVPUTFAIL);
		assert(gvt_trigger == gv_target->gvt_trigger);
		assert(gv_target == save_targ); /* gv_target should NOT have been mutated by the trigger reads */
		if (is_tpwrap)
		{	/* The above call to GVTR_INIT* macro created a TP transaction (by invoking op_tstart).
			 * Save all pertinent global variable information that needs to be restored in case of
			 * a restart.  Note that the restart could happen in a nested trigger so these global
			 * variables could have changed in value from what they were at gvcst_put entry, hence
			 * the need to save/restore them.  If this is not an implicitly tp wrapped transaction,
			 * there is no need to do this save/restore because a restart will transfer control
			 * back to the M code corresponding to the start of the transaction which would
			 * automatically initialize these global variables to the appropriate values.
			 */
			assert(lcl_implicit_tstart);
			lcl_is_dollar_incr = is_dollar_incr;
			lcl_val = val;
			lcl_val_forjnl = val_forjnl;
			lcl_post_incr_mval = post_incr_mval;
			lcl_increment_delta_mval = increment_delta_mval;
			jnl_format_done = FALSE;
		}
		if (NULL != gvt_trigger)
		{
			PUSH_ZTOLDMVAL_ON_M_STACK(ztold_mval, save_msp, save_mv_chain);
			if (lcl_span_status)
			{	/* Need the ability to pop vals from gvcst_put. */
				parms->ztold_mval = ztold_mval;
				parms->save_msp = save_msp;
				parms->save_mv_chain = (unsigned char *)save_mv_chain;
			}
		}
	}
#	endif
	assert(csd == cs_data);	/* assert csd is in sync with cs_data even if there were MM db file extensions */
	si = sgm_info_ptr;	/* Cannot be moved before GVTR_INIT_AND_TPWRAP_IF_NEEDED macro since we could enter gvcst_put
				 * with sgm_info_ptr NULL but could tpwrap a non-tp transaction due to triggers. In that case
				 * we want the updated sgm_info_ptr to be noted down in si and used later.
				 */
	assert((NULL == si) || (si->update_trans));
	assert(NULL != update_array);
	assert(NULL != update_array_ptr);
	assert(0 != update_array_size);
	assert(update_array + update_array_size >= update_array_ptr);
	/* When the following two asserts trip, we should change the data types of prev_first_off
	 * and prev_next_off, so they satisfy the assert.
	 */
	assert(SIZEOF(prev_first_off) >= SIZEOF(block_offset));
	assert(SIZEOF(prev_next_off) >= SIZEOF(block_offset));
	prev_first_off = prev_next_off = PREV_OFF_INVALID;
	horiz_growth = FALSE;
	assert(t_tries < CDB_STAGNATE || csa->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
	/* level_0 == true and no_pointers == false means that this is a directory tree data block containing pointers to roots */
	level_0 = no_pointers = TRUE;
	assert(gv_altkey->top == gv_currkey->top);
	assert(gv_altkey->top == gv_keysize);
	assert(gv_currkey->end < gv_currkey->top);
	assert(gv_altkey->end < gv_altkey->top);
	temp_key = gv_currkey;
	dir_hist = NULL;
	ins_chain_index = 0;
	lcl_root = gv_target->root;
	tp_root = lcl_root;
	if (!dollar_tlevel)
	{
		CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	} else
	{
		segment_update_array_size = UA_NON_BM_SIZE(csd);
		ENSURE_UPDATE_ARRAY_SPACE(segment_update_array_size);
		curr_chain = *(off_chain *)&lcl_root;
		if (curr_chain.flag == 1)
		{
			tp_get_cw(si->first_cw_set, (int)curr_chain.cw_index, &cse);
			tp_root = cse->blk;
			assert(tp_root);
		}
	}
	if (0 == tp_root)
	{	/* Global does not exist as far as we know. Creating a new one requires validating the directory tree path which
		 * led us to this conclusion. So scan the directory tree here and validate its history at the end of this function.
		 * If we decide to restart due to a concurrency conflict, remember to reset gv_target->root to 0 before restarting.
		 */
		gv_target = dir_tree = csa->dir_tree;
		SET_GV_ALTKEY_TO_GBLNAME_FROM_GV_CURRKEY;	/* set up gv_altkey to be just the gblname */
		dir_hist = &gv_target->hist;
		status = gvcst_search(gv_altkey, NULL);
		RESET_GV_TARGET_LCL(save_targ);
		if (cdb_sc_normal != status)
			GOTO_RETRY;
		if (gv_altkey->end + 1 == dir_hist->h[0].curr_rec.match)
		{
			tmp_rp = (rec_hdr_ptr_t)(dir_hist->h[0].buffaddr + dir_hist->h[0].curr_rec.offset);
			EVAL_CMPC2(tmp_rp, tmp_cmpc);
			GET_LONG(tp_root, (dir_hist->h[0].buffaddr + SIZEOF(rec_hdr)
					   + dir_hist->h[0].curr_rec.offset + gv_altkey->end + 1 - tmp_cmpc));
			if (dollar_tlevel)
			{
				gvt_for_root = dir_hist->h[0].blk_num;
				curr_chain = *(off_chain *)&gvt_for_root;
				if (curr_chain.flag == 1)
					tp_get_cw(si->first_cw_set, curr_chain.cw_index, &cse);
				else
				{
					if (NULL != (tabent = lookup_hashtab_int4(si->blks_in_use, (uint4 *)&gvt_for_root)))
						tp_srch_status = tabent->value;
					else
						tp_srch_status = NULL;
					cse = tp_srch_status ? tp_srch_status->cse : NULL;
				}
				assert(!cse || !cse->high_tlevel);
			}
			assert(0 == gv_target->root);
			gv_target->root = tp_root;
		}
	}
	blk_reserved_size = blk_size - blk_reserved_bytes;
	if (0 == tp_root)
	{	/* there is no entry in the GVT (and no root), so create a new empty tree and put the name in the GVT */
		/* Create the data block */
		key_exists = FALSE;
		if (is_dollar_incr)
		{	/* The global variable that is being $INCREMENTed does not exist.
			 * $INCREMENT() should not signal UNDEF error but proceed with an implicit $GET().
			 */
			assert(dollar_tlevel ? si->update_trans : update_trans);
			*post_incr_mval = *val;
			MV_FORCE_NUM(post_incr_mval);
			post_incr_mval->mvtype &= ~MV_STR;	/* needed to force any alphanumeric string to numeric */
			MV_FORCE_STR(post_incr_mval);
			assert(post_incr_mval->str.len);
			value = post_incr_mval->str;
			/* The MAX_REC_SIZE check could not be done in op_gvincr (like is done in op_gvput) because
			 * the post-increment value is not known until here. so do the check here.
			 */
			ENSURE_VALUE_WITHIN_MAX_REC_SIZE(value, dir_tree);
		} else
			value = val->str;
		/* Potential size of a GVT leaf block containing just the new/updated record */
		new_blk_size_single = SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + temp_key->end + 1 + value.len;
		if (new_blk_size_single > blk_reserved_size)
		{	/* The record that is newly inserted/updated does not fit by itself in a separate block
			 * if the current reserved-bytes for this database is taken into account. Cannot go on.
			 */
			ISSUE_RSVDBYTE2HIGH_ERROR(dir_tree);
		}
		BLK_ADDR(curr_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
		curr_rec_hdr->rsiz = SIZEOF(rec_hdr) + temp_key->end + 1 + value.len;
		SET_CMPC(curr_rec_hdr, 0);
		BLK_INIT(bs_ptr, new_blk_bs);
		BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, SIZEOF(rec_hdr));
		BLK_ADDR(cp1, temp_key->end + 1, unsigned char);
		memcpy(cp1, temp_key->base, temp_key->end + 1);
		BLK_SEG(bs_ptr, cp1, temp_key->end + 1);
		if (0 != value.len)
		{
			BLK_ADDR(va, value.len, char);
			memcpy(va, value.addr, value.len);
			BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
		}
		if (0 == BLK_FINI(bs_ptr, new_blk_bs))
		{
			assert(CDB_STAGNATE > t_tries);
			status = cdb_sc_mkblk;
			GOTO_RETRY;
		}
		assert(new_blk_bs[0].len <= blk_reserved_size); /* Assert that new block has space for reserved bytes */
		/* Create the index block */
		BLK_ADDR(curr_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
		curr_rec_hdr->rsiz = BSTAR_REC_SIZE;
		SET_CMPC(curr_rec_hdr, 0);
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, SIZEOF(rec_hdr));
		BLK_SEG(bs_ptr, (unsigned char *)&zeroes, SIZEOF(block_id));
		if (0 == BLK_FINI(bs_ptr, bs1))
		{
			assert(CDB_STAGNATE > t_tries);
			status = cdb_sc_mkblk;
			GOTO_RETRY;
		}
		assert(bs1[0].len <= blk_reserved_size); /* Assert that new block has space for reserved bytes */
        	allocation_clue = ALLOCATION_CLUE(csd->trans_hist.total_blks);
		next_blk_index = t_create(allocation_clue, (uchar_ptr_t)new_blk_bs, 0, 0, 0);
		++allocation_clue;
		ins_chain_index = t_create(allocation_clue, (uchar_ptr_t)bs1, SIZEOF(blk_hdr) + SIZEOF(rec_hdr), next_blk_index, 1);
		root_blk_cw_index = ins_chain_index;
		temp_key = gv_altkey;
		gv_target->hist.h[0].blk_num = HIST_TERMINATOR;
		gv_target = dir_tree;
		bh = &gv_target->hist.h[0];
		value.len = SIZEOF(block_id);
		value.addr = (char *)&zeroes;
		no_pointers = FALSE;
	} else
	{
#		ifdef GTM_TRIGGER
		if (lcl_span_status && (NULL != ztold_mval) && !skip_hasht_read && parms->enable_trigger_read_and_fire)
		{	/* Dealing with spanning nodes, need to use a get routine to find ztold_val. Need to do this BEFORE
			 * gvcst_search below or we'll disrupt gv_target->hist, which is used in the subsequent constructions.
			 * Though we don't need dollar_data, we use gvcst_dataget since it returns status, allowing retry
			 * cleanup to be done (RESET_GV_TARGET_LCL_AND_CLR_GBL in particular) before invoking a restart.
			 */
			assert(dollar_tlevel && !skip_dbtriggers);
			dlr_data = DG_GETONLY; /* tell dataget we just want to do a get; we don't care about descendants */
			status = gvcst_dataget(&dlr_data, ztold_mval);
			if (cdb_sc_normal != status)
				GOTO_RETRY;
		}
#		endif
#if defined(DEBUG) && defined(UNIX)
		if (gtm_white_box_test_case_enabled && (WBTEST_ANTIFREEZE_GVINCRPUTFAIL == gtm_white_box_test_case_number))
		{
			status = cdb_sc_blknumerr;
			GOTO_RETRY;
		}
#endif
		if (cdb_sc_normal != (status = gvcst_search(gv_currkey, NULL)))
			GOTO_RETRY;
		target_key_size = gv_currkey->end + 1;
		bh = &gv_target->hist.h[0];
		key_exists = (target_key_size == bh->curr_rec.match);
#		ifdef UNIX
		if (key_exists)
		{	/* check for spanning node dummy value: a single zero byte */
			rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)bh->buffaddr + bh->curr_rec.offset);
			GET_USHORT(rec_size, &rp->rsiz);
			cur_val_offset = SIZEOF(rec_hdr) + target_key_size - EVAL_CMPC((rec_hdr_ptr_t)rp);
			data_len = rec_size - cur_val_offset;
			is_dummy = (1 == data_len) && ('\0' == *(sm_uc_ptr_t)((sm_uc_ptr_t)rp + cur_val_offset));
			if (is_dummy && !lcl_span_status && (csa->dir_tree != gv_target)
						&& !(span_nodes_disallowed && csd->span_node_absent))
			{	/* Validate that value is really $zchar(0) and either restart or back out of gvcst_put2.
				 * Three cases:
				 * 1)	not in TP, no triggers
				 * 2)	in TP (triggers invoked or not)
				 * 3)	weren't in TP when we entered gvcst_put2, but did an op_tstart to check for triggers
				 */
				RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ, DO_GVT_GVKEY_CHECK);
				if (!dollar_tlevel)
				{
					update_trans = 0;
					succeeded = ((trans_num)0 != t_end(&gv_target->hist, dir_hist, TN_NOT_SPECIFIED));
					if (!succeeded)
					{	/* see other t_end */
						RESTORE_ZERO_GVT_ROOT_ON_RETRY(lcl_root, gv_target, dir_hist, dir_tree);
						jnl_format_done = !needfmtjnl;
						GTMTRIG_DBG_ONLY(dbg_trace_array[dbg_num_iters].retry_line = __LINE__);
						update_trans = UPDTRNS_DB_UPDATED_MASK;
						goto tn_restart;
					}
				} else
				{
					status = tp_hist(dir_hist);
					if (NULL != dir_hist)
						ADD_TO_GVT_TP_LIST(dir_tree, RESET_FIRST_TP_SRCH_STATUS_FALSE);
					if (cdb_sc_normal != status)
						GOTO_RETRY;
#					ifdef GTM_TRIGGER
					if (lcl_implicit_tstart)
					{	/* Started TP for playing triggers. Abort and try again outside after nsb op_tstart
						 * Otherwise, we were already in TP when we came into gvcst_put. Triggers can stay,
						 * but finish put outside.
						 */
						POP_MVALS_FROM_M_STACK_IF_NEEDED(ztold_mval, save_msp, save_mv_chain);
						OP_TROLLBACK(-1);
					}
#					endif
				}
				parms->span_status = TRUE;
				return;
			}
		}
#		endif
		if (is_dollar_incr)
		{
			if (key_exists)
			{	/* $INCR is being done on an existing global variable key in the database.
				 * the value to set the key to has to be determined by adding the existing value
				 * with the increment passed as the input parameter "val" (of type (mval *)) to gvcst_put
				 */
				if (cdb_sc_normal != (status = gvincr_compute_post_incr(bh)))
				{
					assert(CDB_STAGNATE > t_tries);
					GOTO_RETRY;
				}
			} else
			{	/* The global variable that is being $INCREMENTed does not exist.  $INCREMENT() should not
				 * signal UNDEF error but proceed with an implicit $GET() */
				*post_incr_mval = *val;
				MV_FORCE_NUM(post_incr_mval);
				post_incr_mval->mvtype &= ~MV_STR;	/* needed to force any alphanumeric string to numeric */
				MV_FORCE_STR(post_incr_mval);
				assert(post_incr_mval->str.len);
			}
			assert(MV_IS_STRING(post_incr_mval));
			assert(dollar_tlevel ? si->update_trans : update_trans);
			value = post_incr_mval->str;
			/* The MAX_REC_SIZE check could not be done in op_gvincr (like is done in op_gvput) because
			 * the post-increment value is not known until here. so do the check here.
			 */
			ENSURE_VALUE_WITHIN_MAX_REC_SIZE(value, gv_target);

		} else
			value = val->str;
	}
	/* --------------------------------------------------------------------------------------------
	 * The code for the non-block-split case is very similar to the code in recompute_upd_array.
	 * Any changes in either place should be reflected in the other.
	 * --------------------------------------------------------------------------------------------
	 */
	need_extra_block_split = FALSE; /* Assume we don't require an additional block split (most common case) */
	duplicate_set = FALSE; /* Assume this is NOT a duplicate set (most common case) */
	split_depth = 0;
	split_targ = gv_target;
	for (succeeded = FALSE; !succeeded; no_pointers = level_0 = FALSE)
	{
		buffaddr = bh->buffaddr;
		cur_blk_size = ((blk_hdr_ptr_t)buffaddr)->bsiz;
		target_key_size = temp_key->end + 1;
		/* Potential size of a block containing just the new/updated record */
		new_blk_size_single = SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + target_key_size + value.len;
		if (new_blk_size_single > blk_reserved_size)
		{	/* The record that is newly inserted/updated does not fit by itself in a separate block
			 * if the current reserved-bytes for this database is taken into account. If this is not a
			 * GVT leaf block, this situation is then possible if we are not in the final retry (and hence
			 * dont hold crit on the region) and "temp_key->end" (and in turn "target_key_size") was
			 * computed from a stale copy (due to concurrent updates or buffer reuse) of the global buffer
			 * (effectively a restartable situation). If so, restart. If not issue error.
			 */
			if (no_pointers || (CDB_STAGNATE <= t_tries))
			{
				ISSUE_RSVDBYTE2HIGH_ERROR(gv_target);
			} else
			{
				status = cdb_sc_mkblk;
				GOTO_RETRY;
			}
		}
		curr_rec_match = bh->curr_rec.match;
		curr_rec_offset = bh->curr_rec.offset;
		new_rec = (target_key_size != curr_rec_match);
                if (!new_rec && !no_pointers)
                {
                        assert(CDB_STAGNATE > t_tries);
                        status = cdb_sc_lostcr;         /* will a new cdb_sc status be better */
                        GOTO_RETRY;
                }
		rp = (rec_hdr_ptr_t)(buffaddr + curr_rec_offset);
		if (curr_rec_offset == cur_blk_size)
		{
			if ((FALSE == new_rec) && dollar_tlevel)
			{
				assert(CDB_STAGNATE > t_tries);
				status = cdb_sc_mkblk;
				GOTO_RETRY;
			}
			rec_cmpc = 0;
			rec_size = 0;
		} else
		{
			GET_USHORT(rec_size, &rp->rsiz);
			rec_cmpc = EVAL_CMPC(rp);
			if ((sm_uc_ptr_t)rp + rec_size > (sm_uc_ptr_t)buffaddr + cur_blk_size)
			{
				assert(CDB_STAGNATE > t_tries);
				status = cdb_sc_mkblk;
				GOTO_RETRY;
			}
		}
		prev_rec_match = bh->prev_rec.match;
		if (new_rec)
		{
			new_rec_size = SIZEOF(rec_hdr) + target_key_size - prev_rec_match + value.len;
			if (cur_blk_size <= (signed int)curr_rec_offset) /* typecast necessary to enforce "signed int" comparison */
				next_rec_shrink = 0;
			else
				next_rec_shrink = curr_rec_match - rec_cmpc;
			delta = new_rec_size - next_rec_shrink;
		} else
		{
			if (rec_cmpc != prev_rec_match)
			{
				assert(CDB_STAGNATE > t_tries);
				status = cdb_sc_mkblk;
				GOTO_RETRY;
			}
			assert(target_key_size > rec_cmpc);
			cur_val_offset = SIZEOF(rec_hdr) + (target_key_size - rec_cmpc);
#			ifdef GTM_TRIGGER
			if (no_pointers && (NULL != ztold_mval) && !skip_hasht_read && parms->enable_trigger_read_and_fire
					&& !lcl_span_status)
			{	/* Complete initialization of ztold_mval */
				assert(!skip_dbtriggers);
				data_len = rec_size - cur_val_offset;
				if (0 > data_len)
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_rmisalign;
					GOTO_RETRY;
				}
				ztold_mval->str.len = data_len;
				if (data_len)
				{
					if (!(IS_STP_SPACE_AVAILABLE(data_len)))
					{
						PUSH_MV_STENT(MVST_MVAL);       /* protect "value" mstr from stp gcol */
						pval = &mv_chain->mv_st_cont.mvs_mval;
						pval->str = value;
						ENSURE_STP_FREE_SPACE(data_len);
						value = pval->str;
						POP_MV_STENT();                 /* pval */
					}
					ztold_mval->str.addr = (char *)stringpool.free;
					memcpy(ztold_mval->str.addr, (sm_uc_ptr_t)rp + cur_val_offset, data_len);
					stringpool.free += data_len;
				}
				ztold_mval->mvtype = MV_STR;	/* ztold_mval is now completely initialized */
			}
#			endif
			new_rec_size = cur_val_offset + value.len;
			delta = new_rec_size - rec_size;
			if (!delta && value.len
				&& !memcmp(value.addr, (sm_uc_ptr_t)rp + new_rec_size - value.len, value.len))
			{
				duplicate_set = TRUE;
				if (gvdupsetnoop)
				{	/* We do not want to touch the DB Blocks in case of a duplicate set unless the
					 * dupsetnoop optimization is disabled. Since it is enabled, let us break right away.
					 */
					succeeded = TRUE;
					break;	/* duplicate SET */
				}
			}
			next_rec_shrink = 0;
		}
		blk_num = bh->blk_num;
		bh_level = bh->level;
		if (dollar_tlevel)
		{
			if ((SIZEOF(rec_hdr) + target_key_size - prev_rec_match + value.len) != new_rec_size)
			{
				assert(CDB_STAGNATE > t_tries);
				status = cdb_sc_mkblk;
				GOTO_RETRY;
			}
			chain1 = *(off_chain *)&blk_num;
			if ((1 == chain1.flag) && ((int)chain1.cw_index >= si->cw_set_depth))
			{
				assert(si->tp_csa == csa);
				assert(FALSE == csa->now_crit);
				status = cdb_sc_blknumerr;
				GOTO_RETRY;
			}
		}
		next_rec_shrink1 = next_rec_shrink;
		/* Potential size of the current block including the new/updated record */
		new_blk_size = cur_blk_size + delta;
		/* It is possible due to concurrency issues (for example if the buffer that we are planning on updating
		 * in shared memory got reused for a different block) that "new_blk_size" is lesser than "new_blk_size_single"
		 * In those cases, we will go into the non-block-split case but eventually we will restart.
		 */
		assert((new_blk_size >= new_blk_size_single) || (CDB_STAGNATE > t_tries));
		if ((new_blk_size <= blk_fill_size) || (new_blk_size <= new_blk_size_single))
		{	/* Update can be done without overflowing the block's fillfactor OR the record to be updated
			 * is the only record in the new block. Do not split block in either case. This means we might
			 * not honour the desired FillFactor if the only record in a block exceeds the blk_fill_size,
			 * but in this case we are guaranteed the block has room for the current reserved bytes.
			 */
			if (no_pointers)	/* level zero (normal) data block: no deferred pointer chains */
				ins_chain_offset = 0;
			else			/* index or directory level block */
				ins_chain_offset =(int)((sm_uc_ptr_t)rp - buffaddr + new_rec_size - SIZEOF(block_id));
			BLK_INIT(bs_ptr, bs1);
			if (0 == rc_set_fragment)
			{
				BLK_SEG(bs_ptr, buffaddr + SIZEOF(blk_hdr), curr_rec_offset - SIZEOF(blk_hdr));
				BLK_ADDR(curr_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
				curr_rec_hdr->rsiz = new_rec_size;
				SET_CMPC(curr_rec_hdr, prev_rec_match);
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, SIZEOF(rec_hdr));
				BLK_ADDR(cp1, target_key_size - prev_rec_match, unsigned char);
				memcpy(cp1, temp_key->base + prev_rec_match, target_key_size - prev_rec_match);
				BLK_SEG(bs_ptr, cp1, target_key_size - prev_rec_match);
				if (0 != value.len)
				{
					BLK_ADDR(va, value.len, char);
					memcpy(va, value.addr, value.len);
					BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
				}
				if (!new_rec)
					rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + rec_size);
				n = (int)(cur_blk_size - ((sm_uc_ptr_t)rp - buffaddr));
				if (n > 0)
				{
					if (new_rec)
					{
						BLK_ADDR(next_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
						next_rec_hdr->rsiz = rec_size - next_rec_shrink;
						SET_CMPC(next_rec_hdr, curr_rec_match);
						BLK_SEG(bs_ptr, (sm_uc_ptr_t)next_rec_hdr, SIZEOF(rec_hdr));
						next_rec_shrink += SIZEOF(rec_hdr);
					}
					if (n >= next_rec_shrink)
					{
						BLK_SEG(bs_ptr, (sm_uc_ptr_t)rp + next_rec_shrink, n - next_rec_shrink);
					} else
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_mkblk;
						GOTO_RETRY;
					}
				}
			} else
			{	/* With GT.M TRIGGERS, it is not clear how the RC protocol will work. The below assert is to
				 * be informed whenever such usage happens (expected to be really rare) and handle it right
				 * then instead of worrying about it during the initial trigger implementation.
				 */
				assert(FALSE);
				curr_rec_hdr = (rec_hdr_ptr_t)(buffaddr + curr_rec_offset);
				EVAL_CMPC2(curr_rec_hdr, tmp_cmpc);
				/* First piece is block prior to record + key + data prior to fragment */
				BLK_SEG(bs_ptr,
					buffaddr + SIZEOF(blk_hdr),
					curr_rec_offset - SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + rc_set_fragment
						+ gv_currkey->end + 1 - tmp_cmpc);
				/* Second piece is fragment itself */
				BLK_ADDR(va, value.len, char);
				memcpy(va, value.addr, value.len);
				BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
				/* Third piece is data after fragment + rest of block after record */
				n = (int)(cur_blk_size - ((sm_uc_ptr_t)curr_rec_hdr - buffaddr) - SIZEOF(rec_hdr)
					- (gv_currkey->end + 1 - tmp_cmpc) - rc_set_fragment - value.len);
				if (0 < n)
					BLK_SEG(bs_ptr,
						(sm_uc_ptr_t)curr_rec_hdr + gv_currkey->end + 1 - tmp_cmpc
							+ rc_set_fragment + value.len,
						n);
			}
			if (0 == BLK_FINI(bs_ptr, bs1))
			{
				assert(CDB_STAGNATE > t_tries);
				status = cdb_sc_mkblk;
				GOTO_RETRY;
			}
			assert(bs1[0].len <= blk_reserved_size); /* Assert that new block has space for reserved bytes */
			cse = t_write(bh, (unsigned char *)bs1, ins_chain_offset, ins_chain_index, bh_level,
				FALSE, FALSE, GDS_WRITE_PLAIN);
			assert(!dollar_tlevel || !cse->high_tlevel);
			if ((0 != ins_chain_offset) && (NULL != cse) && (0 != cse->first_off))
			{	/* formerly tp_offset_chain - inserts a new_entry in the chain */
				assert((NULL != cse->new_buff) || horiz_growth && cse->low_tlevel->new_buff
									&& (buffaddr == cse->low_tlevel->new_buff));
				assert(0 == cse->next_off);
				assert(ins_chain_offset > (signed)SIZEOF(blk_hdr));	/* we want signed comparison */
				assert((curr_rec_offset - SIZEOF(off_chain)) == (ins_chain_offset - new_rec_size));
				offset_sum = cse->first_off;
				curr = buffaddr + offset_sum;
				/* The typecast is needed below to enforce a "signed int" (versus "unsigned int") comparison */
				if (offset_sum >= (signed int)curr_rec_offset)
				{	/* the new record is prior to the first existing chain record, id the new one as first */
					/* first_off-------------v--------------------v
					 * [blk_hdr]...[new rec ( )]...[existing rec ( )]... */
					cse->next_off = cse->first_off - (ins_chain_offset - new_rec_size) - next_rec_shrink1;
					cse->first_off = ins_chain_offset;
				} else
				{
					if (horiz_growth)
					{
						old_cse = cse->low_tlevel;
						assert(old_cse->first_off);
						assert(old_cse && old_cse->done);
						assert(!old_cse->undo_next_off[0] && !old_cse->undo_offset[0]);
					}
					/* find chain records before and after the new one */
					for ( ; ; curr += curr_chain.next_off)
					{	/* try to make offset_sum identify the first chain entry after the new record */
						GET_LONGP(&curr_chain, curr);
						assert(curr_chain.flag == 1);
						if (0 == curr_chain.next_off)
							break;
						offset_sum += curr_chain.next_off;
						/* The typecast is needed below to enforce a "signed int" comparison */
						if (offset_sum >= (signed int)curr_rec_offset)
							break;
					}
					/* store the next_off in old_cse before changing it in the buffer (for rolling back) */
					if (horiz_growth)
					{
						old_cse->undo_next_off[0] = curr_chain.next_off;
						old_cse->undo_offset[0] = (block_offset)(curr - buffaddr);
						assert(old_cse->undo_offset[0]);
					}
					if (0 == curr_chain.next_off)
					{	/* the last chain record precedes the new record: just update it */
						/* 			   ---|---------------v
						 * [blk_hdr]...[existing rec ( )]...[new rec ( )]... */
						curr_chain.next_off = ins_chain_offset - offset_sum;
						GET_LONGP(curr, &curr_chain);
					} else
					{	/* update the chain record before the new one */
						/* 			   ---|---------------v--------------------v
						 * [blk_hdr]...[existing rec ( )]...[new rec ( )]...[existing rec ( )] */
						curr_chain.next_off = (unsigned int)(ins_chain_offset - (curr - buffaddr));
						GET_LONGP(curr, &curr_chain);
						cse->next_off = offset_sum - (ins_chain_offset - new_rec_size) - next_rec_shrink1;
					}
				}
				assert((ins_chain_offset + (int)cse->next_off) <=
				       (delta + (sm_long_t)cur_blk_size - SIZEOF(off_chain)));
			}
			succeeded = TRUE;
			if (level_0)
			{
				if (new_rec)
				{	/* New record insertion at leaf level. gvcst_search would have already updated clue to
					 * reflect the new key, but we need to fix the search history to keep it in sync with clue.
					 * This search history (and clue) will be used by the NEXT call to gvcst_search.
					 * Note that clue.end could be 0 at this point (see "Clue less than first rec, invalidate"
					 * comment in gvcst_search) in which case the below assignment is unnecessary (though does
					 * not hurt) but we want to avoid the if check (since we expect clue to be non-zero mostly).
					 */
					assert((0 == gv_target->clue.end) || (gv_target->clue.end + 1 == target_key_size));
					assert(1 < target_key_size);
					assert(bh->curr_rec.match != target_key_size);
					bh->curr_rec.match = target_key_size;
				}
				/* -------------------------------------------------------------------------------------------------
				 * We have to maintain information for future recomputation only if the following are satisfied
				 *	1) The block is a leaf-level block
				 *	2) We are in TP (indicated by non-null cse)
				 *	3) The global has NOISOLATION turned ON
				 *	4) The cw_set_element hasn't encountered a block-split or a kill
				 *	5) We don't need an extra_block_split
				 *
				 * We can also add an optimization that only cse's of mode gds_t_write need to have such updations,
				 *	but because of the belief that for a nonisolated variable, we will very rarely encounter a
				 *	situation where a created block (in TP) will have some new keys added to it, and that adding
				 *	the check slows down the normal code, we don't do that check here.
				 * -------------------------------------------------------------------------------------------------
				 */
				if (cse && gv_target->noisolation && !cse->write_type && !need_extra_block_split)
				{
					assert(dollar_tlevel);
					if (is_dollar_incr)
					{	/* See comment in ENSURE_VALUE_WITHIN_MAX_REC_SIZE macro
						 * definition for why the below macro call is necessary.
						 */
						ADD_TO_GVT_TP_LIST(gv_target, RESET_FIRST_TP_SRCH_STATUS_FALSE);
						rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_GVINCRISOLATION, 2,
							gv_target->gvname.var_name.len, gv_target->gvname.var_name.addr);
					}
					if (NULL == cse->recompute_list_tail ||
						0 != memcmp(gv_currkey->base, cse->recompute_list_tail->key.base,
							gv_currkey->top))
					{
						tempkv = (key_cum_value *)get_new_element(si->recompute_list, 1);
						tempkv->key = *gv_currkey;
						tempkv->next = NULL;
						memcpy(tempkv->key.base, gv_currkey->base, gv_currkey->end + 1);
						if (NULL == cse->recompute_list_head)
						{
							assert(NULL == cse->recompute_list_tail);
							cse->recompute_list_head = tempkv;
						} else
							cse->recompute_list_tail->next = tempkv;
						cse->recompute_list_tail = tempkv;
					} else
						tempkv = cse->recompute_list_tail;
					assert(0 == val->str.len
						|| ((val->str.len == bs1[4].len)
							&& 0 == memcmp(val->str.addr, bs1[4].addr, val->str.len)));
					tempkv->value.len = val->str.len;	/* bs1[4].addr is undefined if val->str.len is 0 */
					tempkv->value.addr = (char *)bs1[4].addr;/* 	but not used in that case, so ok */
				}

			}
		} else
		{	/* Block split required */
			split_depth++;
			gv_target->clue.end = 0;	/* invalidate clue */
			/* Potential size of the left and right blocks, including the new record */
			new_blk_size_l = curr_rec_offset + new_rec_size;
			new_blk_size_r = SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + target_key_size + value.len + cur_blk_size
						- curr_rec_offset - (new_rec ? next_rec_shrink : rec_size);
			assert(new_blk_size_single <= blk_reserved_size);
			assert(blk_reserved_size >= blk_fill_size);
			extra_record_orig_size = 0;
			prev_rec_offset = bh->prev_rec.offset;
			/* Decide which side (left or right) the new record goes. Ensure either side has at least one record.
			 * This means we might not honor the desired FillFactor if the only record in a block exceeds the
			 * blk_fill_size, but in this case we are guaranteed the block has room for the current reserved bytes.
			 * The typecast of curr_rec_offset is needed below to enforce a "signed int" comparison.
			 */
			if (new_blk_size_r > blk_fill_size)
			{
				new_rec_goes_to_right = (new_blk_size_r == new_blk_size_single);
				last_split_dir = NEWREC_DIR_FORCED;	/* no choice in split direction */
			} else if (new_blk_size_l > blk_fill_size)
			{
				new_rec_goes_to_right = TRUE;
				last_split_dir = NEWREC_DIR_FORCED;	/* no choice in split direction */
			} else
			{	/* new_rec can go in either direction without any issues of fitting in.
				 * This is where we need to use a few heuristics to ensure good block space utilization.
				 * We note down which direction (left or right) the new record went in after the split.
				 * We use that as the heuristic to identify the direction of data loading and do the
				 * splits accordingly for future updates.
				 */
				last_split_dir = (enum split_dir)gv_target->last_split_direction[bh_level];
				if (NEWREC_DIR_FORCED == last_split_dir)
				{	/* dont have prior information to use heuristic. choose whichever side is less full.
					 * if this turns out to not be the correct choice, we will correct ourselves at the
					 * time of the next block split at the same level.
					 */
					last_split_dir = (new_blk_size_l < new_blk_size_r) ? NEWREC_DIR_LEFT : NEWREC_DIR_RIGHT;
				} else
				{	/* Last block split at this level chose a specific direction for new_rec. See if
					 * that heuristic worked. This is done by checking if the block # that new_rec went
					 * into previously is the same block that is being split now. If so, that means the
					 * previous choice of direction was actually not optimal. So try the other direction now.
					 */
					last_split_bnum = gv_target->last_split_blk_num[bh_level];
					if (dollar_tlevel)
					{
						chain2 = *(off_chain *)&last_split_bnum;
						if (chain1.flag == chain2.flag)
						{
							if (!chain1.flag)
								blk_match = (blk_num == last_split_bnum);
							else
							{
								assert(chain1.cw_index < si->cw_set_depth);
								blk_match = (chain1.cw_index == chain2.cw_index);
							}
						} else
							blk_match = FALSE;
					} else
					{
						DEBUG_ONLY(chain1 = *(off_chain *)&last_split_bnum;)
						assert(!chain1.flag);
						blk_match = (blk_num == last_split_bnum);
					}
					is_split_dir_left = (NEWREC_DIR_LEFT == last_split_dir);
					if (blk_match)	/* switch direction since last choice did not seem to have worked */
						last_split_dir = is_split_dir_left ? NEWREC_DIR_RIGHT : NEWREC_DIR_LEFT;
					else
					{	/* blk# did not match means there is a high likelihood that the current split
						 * is happening in the OTHER sibling block from the previous block split operation
						 * at the same level. There is no easy way of confirming this so we assume the
						 * heuristic is doing its job, unless we see evidence otherwise. And that evidence
						 * is IF the block sizes of the left and right halves dont match the direction of
						 * choice (e.g. if we choose NEWREC_DIR_LEFT, we expect the right block to be
						 * almost full and the left block to be almost empty and vice versa).
						 * In this case too switch the direction.
						 */
						if (is_split_dir_left)
						{
							if (new_blk_size_l > new_blk_size_r)
								last_split_dir = NEWREC_DIR_RIGHT;
						} else
						{
							if (new_blk_size_l < new_blk_size_r)
								last_split_dir = NEWREC_DIR_LEFT;
						}
					}
				}
				new_rec_goes_to_right = (NEWREC_DIR_RIGHT == last_split_dir);
			}
			last_split_direction[bh_level] = (char)last_split_dir;
			if (new_rec_goes_to_right)
			{	/* Left side of this block will be split off into a new block.
				 * The new record and the right side of this block will remain in this block.
				 */
				/* prepare new block */
				BLK_INIT(bs_ptr, bs1);
				if (level_0)
				{
					BLK_SEG(bs_ptr, buffaddr + SIZEOF(blk_hdr), curr_rec_offset - SIZEOF(blk_hdr));
				} else
				{	/* for index records, the record before the split becomes a new *-key */
					/* Note:  If the block split was caused by our appending the new record
					 * to the end of the block, this code causes the record PRIOR to the
					 * current *-key to become the new *-key.
					 */
					BLK_SEG(bs_ptr, buffaddr + SIZEOF(blk_hdr), prev_rec_offset - SIZEOF(blk_hdr));
					BLK_ADDR(new_star_hdr, SIZEOF(rec_hdr), rec_hdr);
					new_star_hdr->rsiz = BSTAR_REC_SIZE;
					SET_CMPC(new_star_hdr, 0);
					BLK_SEG(bs_ptr, (sm_uc_ptr_t)new_star_hdr, SIZEOF(rec_hdr));
					BLK_SEG(bs_ptr, (sm_uc_ptr_t)rp - SIZEOF(block_id), SIZEOF(block_id));
				}
				new_blk_bs = bs1;
				if (0 == BLK_FINI(bs_ptr,bs1))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_mkblk;
					GOTO_RETRY;
				}
				/* We want to assert that the left block has enough space for reserved bytes but
				 * it is possible that it DOES NOT have enough space for reserved bytes if the pre-split
				 * block was previously populated with a very low reserved bytes setting and if the current
				 * reserved bytes setting is much higher than what the chosen split point would free up.
				 * This is an issue waiting to be fixed by C9K01-003221. Until then the following assert
				 * has to remain commented out.
				 *
				 * assert(bs1[0].len <= blk_reserved_size);
				 */
				/* prepare the existing block */
				BLK_INIT(bs_ptr, bs1);
				ins_chain_offset = no_pointers ? 0 : (int)(SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + target_key_size);
				left_hand_offset = left_hand_index
						 = 0;
				if (!new_rec)
					rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + rec_size);
				BLK_ADDR(curr_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
				curr_rec_hdr->rsiz = target_key_size + SIZEOF(rec_hdr) + value.len;
				SET_CMPC(curr_rec_hdr, 0);
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, SIZEOF(rec_hdr));
				BLK_ADDR(cp1, target_key_size, unsigned char);
				memcpy(cp1, temp_key->base, target_key_size);
				BLK_SEG(bs_ptr, cp1, target_key_size);
				if (0 != value.len)
				{
					BLK_ADDR(va, value.len, char);
					memcpy(va, value.addr, value.len);
					BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
				}
				if (buffaddr + cur_blk_size > (sm_uc_ptr_t)rp)
				{
					BLK_ADDR(next_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
					GET_USHORT(next_rec_hdr->rsiz, &rp->rsiz);
					next_rec_hdr->rsiz -= next_rec_shrink;
					SET_CMPC(next_rec_hdr, new_rec ? curr_rec_match : EVAL_CMPC(rp));
					BLK_SEG(bs_ptr, (sm_uc_ptr_t)next_rec_hdr, SIZEOF(rec_hdr));
					next_rec_shrink += SIZEOF(rec_hdr);
					n = cur_blk_size - INTCAST(((sm_uc_ptr_t)rp - buffaddr)) - next_rec_shrink;
					if (0 > n)	/* want signed compare as 'n' can be negative */
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_mkblk;
						GOTO_RETRY;
					}
					BLK_SEG(bs_ptr, (sm_uc_ptr_t)rp + next_rec_shrink, n);
				}
				if (0 == BLK_FINI(bs_ptr, bs1))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_mkblk;
					GOTO_RETRY;
				}
				assert(bs1[0].len <= blk_reserved_size); /* Assert that right block has space for reserved bytes */
				assert(gv_altkey->top == gv_currkey->top);
				assert(gv_altkey->end < gv_altkey->top);
				temp_key = gv_altkey;
				if (cdb_sc_normal != (status = gvcst_expand_key((blk_hdr_ptr_t)buffaddr, prev_rec_offset,
						temp_key)))
					GOTO_RETRY;
			} else
			{	/* Insert in left hand (new) block */
				if (!level_0)
				{	/* In case of an index block, as long as the current record is not a *-record
					 * (i.e. last record in the block) and copying an extra record into the left
					 * block does not cause it to exceed the fill factor, copy an additional record.
					 * Not doing the extra record copy for index blocks (was the case pre-V54002) has
					 * been seen to create suboptimally filled index blocks (as low as 15% fillfactor)
					 * depending on the patterns of updates.
					 */
					assert(new_rec);
					copy_extra_record = ((BSTAR_REC_SIZE != rec_size)
									&& ((new_blk_size_l + BSTAR_REC_SIZE) <= blk_fill_size));
				} else
				{
					copy_extra_record = ((0 == prev_rec_offset) && (NEWREC_DIR_LEFT == last_split_dir)
								&& new_rec && (SIZEOF(blk_hdr) < cur_blk_size));
				}
				BLK_INIT(bs_ptr, bs1);
				if (no_pointers)
					left_hand_offset = 0;
				else
				{
					left_hand_offset = curr_rec_offset + SIZEOF(rec_hdr);
					if (level_0 || copy_extra_record)
						left_hand_offset += target_key_size - prev_rec_match;
				}
				left_hand_index = ins_chain_index;
				ins_chain_index = ins_chain_offset = 0;
				BLK_SEG(bs_ptr, buffaddr + SIZEOF(blk_hdr), curr_rec_offset - SIZEOF(blk_hdr));
				if (level_0)
				{	/* After the initial split, will this record fit into the new left block?
					 * If not, this pass will make room and we will do another block split on the next pass.
					 */
					assert((blk_seg_cnt + SIZEOF(rec_hdr) + target_key_size - prev_rec_match + value.len)
						== new_blk_size_l);
					assert((new_blk_size_single <= new_blk_size_l) || (CDB_STAGNATE > t_tries));
					assert((new_blk_size_single != new_blk_size_l)
						|| ((0 == prev_rec_offset) && (SIZEOF(blk_hdr) == curr_rec_offset)));
					assert((new_blk_size_single >= new_blk_size_l)
						|| ((SIZEOF(blk_hdr) <= prev_rec_offset) && (SIZEOF(blk_hdr) < curr_rec_offset)));
					if ((new_blk_size_l > blk_fill_size) && (new_blk_size_l > new_blk_size_single))
					{	/* There is at least one existing record to the left of the split point.
						 * Do the initial split this pass and make an extra split next pass.
						 */
						need_extra_block_split = TRUE;
						DEBUG_ONLY(dbg_trace_array[dbg_num_iters].is_extra_block_split = TRUE;)
					} else
					{
						BLK_ADDR(curr_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
						curr_rec_hdr->rsiz = new_rec_size;
						SET_CMPC(curr_rec_hdr, prev_rec_match);
						BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, SIZEOF(rec_hdr));
						BLK_ADDR(cp1, target_key_size - prev_rec_match, unsigned char);
						memcpy(cp1, temp_key->base + prev_rec_match, target_key_size - prev_rec_match);
						BLK_SEG(bs_ptr, cp1, target_key_size - prev_rec_match);
						if (0 != value.len)
						{
							BLK_ADDR(va, value.len, char);
							memcpy(va, value.addr, value.len);
							BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
						}
						if (copy_extra_record)
						{
							n = rec_size - curr_rec_match;
			 				/* typecast needed below to enforce a "signed int" comparison */
							if ((n + (signed int)curr_rec_offset + new_rec_size) > blk_fill_size)
								copy_extra_record = FALSE;
							else
							{
								BLK_ADDR(extra_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
								extra_rec_hdr->rsiz = n;
								SET_CMPC(extra_rec_hdr, curr_rec_match);
								BLK_SEG(bs_ptr, (sm_uc_ptr_t)extra_rec_hdr, SIZEOF(rec_hdr));
								if (n < (signed)SIZEOF(rec_hdr)) /* want signed compare */
								{				     /* as 'n' can be negative */
									assert(CDB_STAGNATE > t_tries);
									status = cdb_sc_mkblk;
									GOTO_RETRY;
								}
								BLK_SEG(bs_ptr,
									buffaddr + SIZEOF(blk_hdr) + SIZEOF(rec_hdr)
										+ curr_rec_match,
									n - SIZEOF(rec_hdr));
								new_blk_size_l += n;
							}
						}
					}
				} else
				{
					if (copy_extra_record)
					{
						BLK_ADDR(curr_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
						curr_rec_hdr->rsiz = new_rec_size;
						SET_CMPC(curr_rec_hdr, prev_rec_match);
						BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, SIZEOF(rec_hdr));
						BLK_ADDR(cp1, target_key_size - prev_rec_match, unsigned char);
						memcpy(cp1, temp_key->base + prev_rec_match, target_key_size - prev_rec_match);
						BLK_SEG(bs_ptr, cp1, target_key_size - prev_rec_match);
						assert(value.len);
						BLK_ADDR(va, value.len, char);
						memcpy(va, value.addr, value.len);
						BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
						new_blk_size_l += BSTAR_REC_SIZE;
					} else
						new_blk_size_l = curr_rec_offset + BSTAR_REC_SIZE;
					BLK_ADDR(new_star_hdr, SIZEOF(rec_hdr), rec_hdr);
					new_star_hdr->rsiz = BSTAR_REC_SIZE;
					SET_CMPC(new_star_hdr, 0);
					BLK_SEG(bs_ptr, (sm_uc_ptr_t)new_star_hdr, SIZEOF(rec_hdr));
					if (!copy_extra_record)
					{
						BLK_SEG(bs_ptr, (unsigned char *)&zeroes, SIZEOF(block_id));
					} else
						BLK_SEG(bs_ptr, (sm_uc_ptr_t)rp + rec_size - SIZEOF(block_id), SIZEOF(block_id));
				}
				new_blk_bs = bs1;
				if (0 == BLK_FINI(bs_ptr, bs1))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_mkblk;
					GOTO_RETRY;
				}
				/* We want to assert that the left block has enough space for reserved bytes but
				 * it is possible that it DOES NOT have enough space for reserved bytes if the pre-split
				 * block was previously populated with a very low reserved bytes setting and if the current
				 * reserved bytes setting is much higher than what the chosen split point would free up.
				 * This is an issue waiting to be fixed by C9K01-003221. Until then the following assert
				 * has to remain commented out.
				 *
				 * assert(bs1[0].len <= blk_reserved_size);
				 */
				/* assert that both !new_rec and copy_extra_record can never be TRUE at the same time */
				assert(new_rec || !copy_extra_record);
				if (!new_rec || copy_extra_record)
				{	/* Should guard for empty block??? */
					rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + rec_size);
					rec_cmpc = EVAL_CMPC(rp);
					temp_short = rec_size;
					GET_USHORT(rec_size, &rp->rsiz);
				}
				if (copy_extra_record)
				{
				        extra_record_orig_size = temp_short;
					assert(gv_altkey->top == gv_currkey->top);
					assert(gv_altkey->end < gv_altkey->top);
					temp_key = gv_altkey;
					if (cdb_sc_normal !=
						(status = gvcst_expand_key((blk_hdr_ptr_t)buffaddr, curr_rec_offset, temp_key)))
						GOTO_RETRY;
				} else if (temp_key != gv_altkey)
				{
					memcpy(gv_altkey, temp_key, SIZEOF(gv_key) + temp_key->end);
					temp_key = gv_altkey;
				}
				rec_size += rec_cmpc;
				BLK_INIT(bs_ptr, bs1);
				BLK_ADDR(next_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
				next_rec_hdr->rsiz = rec_size;
				SET_CMPC(next_rec_hdr, 0);
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)next_rec_hdr, SIZEOF(rec_hdr));
				BLK_ADDR(cp1, rec_cmpc, unsigned char);
				memcpy(cp1, temp_key->base, rec_cmpc);
				BLK_SEG(bs_ptr, cp1, rec_cmpc);
				n = cur_blk_size - INTCAST(((sm_uc_ptr_t)rp - buffaddr)) - SIZEOF(rec_hdr);
				if (0 > n)	/* want signed compare as 'n' can be negative */
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_mkblk;
					GOTO_RETRY;
				}
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)(rp + 1), n);
				if (0 == BLK_FINI(bs_ptr, bs1))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_mkblk;
					GOTO_RETRY;
				}
				/* We want to assert that the right block has enough space for reserved bytes but
				 * it is possible that it DOES NOT have enough space for reserved bytes if the pre-split
				 * block was previously populated with a very low reserved bytes setting and if the current
				 * reserved bytes setting is much higher than what the chosen split point would free up.
				 * This is an issue waiting to be fixed by C9K01-003221. Until then the following assert
				 * has to remain commented out.
				 *
				 * assert(bs1[0].len <= blk_reserved_size);
				 */
			}
			next_blk_index = t_create(blk_num, (uchar_ptr_t)new_blk_bs, left_hand_offset, left_hand_index, bh_level);
			if (!no_pointers && dollar_tlevel)
			{	/* there may be chains */
				assert(new_rec);
				curr_chain = *(off_chain *)&blk_num;
				if (curr_chain.flag == 1)
					tp_get_cw(si->first_cw_set, curr_chain.cw_index, &cse);
				else
				{
					if (NULL != (tabent = lookup_hashtab_int4(si->blks_in_use, (uint4 *)&blk_num)))
						tp_srch_status = tabent->value;
					else
						tp_srch_status = NULL;
					cse = tp_srch_status ? tp_srch_status->cse : NULL;
				}
				assert(!cse || !cse->high_tlevel);
			        if ((NULL != cse) && (0 != cse->first_off))
				{	/* there is an existing chain: fix to account for the split */
					assert(NULL != cse->new_buff);
					assert(cse->done);
					assert(0 == cse->next_off);
					cse_new = si->last_cw_set;
					assert(!cse_new->high_tlevel);
					assert(0 == cse_new->next_off);
					assert(0 == cse_new->first_off);
					assert(cse_new->ins_off == left_hand_offset);
					assert(cse_new->index == left_hand_index);
					assert(cse_new->level == cse->level);
					cse_first_off = (int4)cse->first_off;
					offset_sum = cse_first_off;
					curr = buffaddr + offset_sum;
					GET_LONGP(&curr_chain, curr);
					assert(curr_chain.flag == 1);
					last_possible_left_offset = curr_rec_offset + extra_record_orig_size - SIZEOF(off_chain);
					/* some of the following logic used to be in tp_split_chain which was nixed */
					if (offset_sum <= last_possible_left_offset)
					{	/* the split falls within or after the chain; otherwise entire chain stays right */
						assert((cse_first_off < curr_rec_offset)
							|| (cse_first_off == last_possible_left_offset));
						if (left_hand_offset && (curr_rec_offset < cse_first_off))
						{	/* We are inserting the new record (with the to-be-filled child block
							 * number) AND an extra record in the left block and the TP block
							 * chain of the block to be split starts AFTER the new record's offset
							 * in the current block. This means the left block (cse_new) will have a
							 * block chain starting with the newly inserted record's block pointer.
							 */
							cse_new->first_off = left_hand_offset;
						} else
						{
							cse_new->first_off = cse_first_off;
							assert(0 == cse_new->next_off);
						}
						if (level_0)	/* if no *-key issue stop after, rather than at, a match */
							last_possible_left_offset += SIZEOF(off_chain);
						if (offset_sum < last_possible_left_offset)
						{	/* it's not an immediate hit */
							for ( ; ; curr += curr_chain.next_off, GET_LONGP(&curr_chain, curr))
							{	/* follow chain upto split point */
								assert(1 == curr_chain.flag);
								if (0 == curr_chain.next_off)
									break;
								offset_sum += curr_chain.next_off;
								if (offset_sum >= last_possible_left_offset)
									break;
							}	/* end of search chain loop */
						}
						assert(curr >= (buffaddr + cse_first_off));
						if (level_0)	/* restore match point to "normal" */
							last_possible_left_offset -= SIZEOF(off_chain);
						if ((offset_sum == last_possible_left_offset) && !level_0)
						{	/* The last record in the left side of the pre-split block is where
							 * the search stopped. If no extra record copy was done, then this
							 * record will end up BEFORE the inserted record in the post-split
							 * left block. Otherwise this will be AFTER the inserted record.
							 *
							 * In case of copy_extra_record, the extra record will become the *-key
							 *                        ---|------------v-----------------v
							 *     [blk_hdr]...[curr rec( )][new rec ( )] [extra rec (*-key)]
							 *
							 * In case of no extra record copy, the new record will become the *-key
							 *                        ---|-------------------v
							 *     [blk_hdr]...[curr rec( )][new rec (*-key)( )]
							 *
							 * Take this into account during the calculations below.
							 */
							assert(cse_first_off <= last_possible_left_offset);
							if (left_hand_offset)
							{
								assert(!ins_chain_offset);
								if (!extra_record_orig_size && (offset_sum != cse_first_off))
								{	/* bring curr up to the match */
									curr += curr_chain.next_off;
									GET_LONGP(&curr_chain, curr);
								}
								curr_offset = curr - buffaddr;
								undo_index = 0;
								if (curr_offset < curr_rec_offset)
								{	/* The chain starts before the curr_rec_offset. Fix
									 * next_off field from the last element in the chain
									 * before this offset.
									 */
									prev_chain = curr_chain;
									assert(extra_record_orig_size
										|| (BSTAR_REC_SIZE
											== (left_hand_offset - curr_offset)));
									prev_chain.next_off = left_hand_offset - curr_offset;
									assert((curr_offset + prev_chain.next_off)
										<= (new_blk_size_l - SIZEOF(off_chain)));
									if (dollar_tlevel != cse->t_level)
									{
										assert(dollar_tlevel > cse->t_level);
										assert(!cse->undo_next_off[0]
												&& !cse->undo_offset[0]);
										assert(!cse->undo_next_off[1]
												&& !cse->undo_offset[1]);
										cse->undo_next_off[0] = curr_chain.next_off;
										cse->undo_offset[0] = (block_offset)curr_offset;
										undo_index = 1;
									}
									GET_LONGP(curr, &prev_chain);
								}
								if (extra_record_orig_size)
								{
									if (offset_sum != cse_first_off)
									{	/* bring curr up to the match */
										curr += curr_chain.next_off;
										curr_offset += curr_chain.next_off;
										GET_LONGP(&curr_chain, curr);
									}
									if (dollar_tlevel != cse->t_level)
									{
										assert(dollar_tlevel > cse->t_level);
										assert(!cse->undo_next_off[undo_index] &&
											!cse->undo_offset[undo_index]);
										cse->undo_next_off[undo_index] =
													curr_chain.next_off;
										cse->undo_offset[undo_index] =
													(block_offset)curr_offset;
									}
									prev_chain = curr_chain;
									prev_chain.next_off = 0;
									GET_LONGP(curr, &prev_chain);
									cse_new->next_off = BSTAR_REC_SIZE;
								}
								offset_sum += curr_chain.next_off;
							} else
							{
								undo_index = 0;
								/* the last record turns into the *-key */
								if (offset_sum == cse_first_off)
								{	/* it's all there is */
									/* first_off --------------------v
									 * [blk_hdr]...[curr rec (*-key)( )] */
									assert(prev_rec_offset >= SIZEOF(blk_hdr));
									cse_new->first_off = (block_offset)(prev_rec_offset +
													    SIZEOF(rec_hdr));
								} else
								{	/* update the next_off of the previous chain record */
									/*		      ---|--------------------v
									 * [blk_hdr]...[prev rec( )][curr rec (*-key)( )] */
									assert((buffaddr + prev_rec_offset) > curr);
									prev_chain = curr_chain;
									assert((offset_sum - prev_chain.next_off) /* check old */
										== (curr - buffaddr)); /* method equivalent */
									prev_chain.next_off = (unsigned int)(
										(prev_rec_offset + (unsigned int)(SIZEOF(rec_hdr))
										 - (curr - buffaddr)));
									assert((curr - buffaddr + prev_chain.next_off)
										<= ((new_blk_size_l < blk_reserved_size
										? new_blk_size_l : blk_reserved_size)
										- SIZEOF(off_chain)));
									if (dollar_tlevel != cse->t_level)
									{
										assert(dollar_tlevel > cse->t_level);
										assert(!cse->undo_next_off[0]
											&& !cse->undo_offset[0]);
										assert(!cse->undo_next_off[1]
											&& !cse->undo_offset[1]);
										cse->undo_next_off[0] = curr_chain.next_off;
										cse->undo_offset[0] = (block_offset)(curr -
														     buffaddr);
										undo_index = 1;
									}
									GET_LONGP(curr, &prev_chain);
									/* bring curr up to the match */
									curr += curr_chain.next_off;
									GET_LONGP(&curr_chain, curr);
								}
								offset_sum += curr_chain.next_off;
								if (dollar_tlevel != cse->t_level)
								{
									assert(dollar_tlevel > cse->t_level);
									assert(!cse->undo_next_off[undo_index] &&
										!cse->undo_offset[undo_index]);
									cse->undo_next_off[undo_index] = curr_chain.next_off;
									cse->undo_offset[undo_index] = (block_offset)(curr -
														      buffaddr);
								}
								curr_chain.next_off = 0;
								GET_LONGP(curr, &curr_chain);
							}
						} else
						{	/* found the split and no *-key issue: just terminate before the split */
							if (offset_sum == cse_first_off)
								offset_sum += curr_chain.next_off;	/* put it in the lead */
							old_curr_chain_next_off = curr_chain.next_off;
							if (left_hand_offset)
							{	/* there's a new chain rec in left */
								curr_offset = curr - buffaddr;
								if (extra_record_orig_size
									&& (curr_offset == last_possible_left_offset))
								{
									assert(level_0);	/* else *-key issues */
									cse_new->next_off = extra_record_orig_size
													- next_rec_shrink1;
								}
								assert(!ins_chain_offset);
								/* put the new one at the end of the chain */
								/*		      ---|---------------v
								 * [blk_hdr]...[curr rec( )]...[new rec ( )] */
								/* the new rec may or may not be a *-key */
								assert((offset_sum - curr_chain.next_off) == curr_offset);
								assert(left_hand_offset > curr_offset);
								curr_chain.next_off = (block_offset)(left_hand_offset
												- curr_offset);
							} else
								curr_chain.next_off = 0;
							assert((curr - buffaddr + curr_chain.next_off)
									<= ((new_blk_size_l < blk_reserved_size
									? new_blk_size_l : blk_reserved_size) - SIZEOF(off_chain)));
							if (dollar_tlevel != cse->t_level)
							{
								assert(dollar_tlevel > cse->t_level);
								assert(!cse->undo_next_off[0] && !cse->undo_offset[0]);
								assert(!cse->undo_next_off[1] && !cse->undo_offset[1]);
								cse->undo_next_off[0] = old_curr_chain_next_off;
								cse->undo_offset[0] = (block_offset)(curr - buffaddr);
							}
							GET_LONGP(curr, &curr_chain);
						}	/* end of *-key or not alternatives */
						assert((left_hand_offset + (int)cse_new->next_off) <=
							((new_blk_size_l < blk_reserved_size ? new_blk_size_l : blk_reserved_size)
								- SIZEOF(off_chain)));
					}	/* end of buffer and cse_new adjustments */
					prev_first_off = cse_first_off;
					if (ins_chain_offset)
					{	/* if there is a new chain rec in the old block, put it first */
						/* first_off---------v
						 * [blk_hdr][new rec( )]... */
						assert(!left_hand_offset);
						assert(0 == extra_record_orig_size);
						assert(ins_chain_offset >= (SIZEOF(blk_hdr) + SIZEOF(rec_hdr)));
						cse->first_off = ins_chain_offset;
						assert(0 == cse->next_off);
						if (offset_sum > last_possible_left_offset)
						{	/* there are existing chain records after the split */
							/* first_off---------v--------------------v
							 * [blk_hdr][new rec( )]...[existing rec ( )] */
							prev_next_off = cse->next_off;
							cse->next_off = offset_sum - last_possible_left_offset - next_rec_shrink1;
							assert((int)(cse->next_off + ins_chain_offset) < new_blk_size_r);
						}
					} else if (offset_sum <= last_possible_left_offset)
					{	/* the last chain record went left with the split */
						cse->first_off = 0;
					} else
					{	/* just adjust the anchor for the split */
						/* first_off------------------v
						 * [blk_hdr]...[existing rec ( )] */
						assert(offset_sum >= (int)cse_first_off);
						cse->first_off =  (block_offset)(offset_sum - last_possible_left_offset + rec_cmpc
								+ SIZEOF(blk_hdr) - SIZEOF(off_chain));
						assert(cse->first_off >= (SIZEOF(blk_hdr) + SIZEOF(rec_hdr)));
					}
					assert((ins_chain_offset + (int)cse->next_off) <=
						((new_blk_size_r < blk_reserved_size ? new_blk_size_r : blk_reserved_size)
							- SIZEOF(off_chain)));
				}	/* end of of split processing */
			}	/* end of tp only code */
			if (!dollar_tlevel)
				cse = NULL;
			else
			{
				cse_new = si->last_cw_set;
				assert(!cse_new->high_tlevel);
				gvcst_blk_build(cse_new, NULL, 0);
				cse_new->done = TRUE;
			}
			/* Record block split heuristic info that will be used in next block split */
			if (!new_rec_goes_to_right)
			{
				chain1.flag = 1;
				chain1.cw_index = next_blk_index;
				chain1.next_off = 0;
				assert(SIZEOF(gv_target->last_split_blk_num[bh_level]) == SIZEOF(off_chain));
				last_split_blk_num[bh_level] = *(block_id *)&chain1;
			} else
				last_split_blk_num[bh_level] = blk_num;
			assert(temp_key == gv_altkey);
			/* If new_rec_goes_to_right is TRUE, then it almost always implies that the left side of
			 * the block is almost full (i.e. adding the new record there caused it to exceed the fill
			 * factor) therefore direct all future updates to keys in between (which lie between the
			 * last key of the left block and the first key of the right block) to the right block.
			 *
			 * If not, direct those updates to the left block thereby preventing it from staying at a
			 * low capacity for a long period of time.
			 *
			 * This direction of future updates is implemented by controlling what key gets passed for
			 * record addition into the parent index block. For directing all in-between updates to the
			 * right block, pass in the last key of the left block to the parent index block. For directing
			 * all in-between updates to the left block, back off 1 spot from the first key of the right
			 * block and pass that to the parent index block.
			 *
			 * Doing this backoff accurately would imply finding the last non-zero byte in the key and taking
			 * 1 off from it. In case the length of the right key is less than the left key, it is possible
			 * that this backoff causes the new key to be less than even the left key (e.g. if left side has
			 * "C2 13 93 00" as key sequence corresponding to the number 1292 and right side has "C2 14 00"
			 * corresponding to the number 1300, taking one off the right side would give "C2 13 00" which corresponds
			 * to the number 12 and is lesser than the left side). In this case, we would have to start adding in
			 * FF bytes to the key as much as possible until we reached the left key length. In the above example,
			 * we would get "C2 13 FF 00".
			 *
			 * In the end, because of the complexities involved in getting an accurate backoff (see above paragraph),
			 * we instead implement a simplified backoff by examining just the first byte that differs and the
			 * immediately following byte (if needed). If it turns out that we cannot get a backoff with just
			 * those 2 bytes (should be rare), we then let the left key go unmodified. In such cases, we expect
			 * not many intervening possible keys and and therefore it does not matter that much whether we pass
			 * the left or (right-1) key to the parent.
			 *
			 * There are two additional cases in which we let the left key go unmodified: 1) if the backoff would
			 * result in a key larger than max_key_size and 2) if the left key ends in "00 00" and the right key ends
			 * in "00 01 ... ". Backing off the 01 would give a index key with "00 00" in the middle.
			 *
			 * temp_key already holds the key corresponding to the last record of the left block.
			 * bs1[2] and bs1[3] hold the key corresponding to the first record of the right block.
			 */
			if (level_0)
			{	/* Determine key for record to pass on to parent index block */
				cp1 = temp_key->base;
				assert(KEY_DELIMITER != *temp_key->base);
				cp2 = (unsigned char *)bs1[2].addr;
				bs1_2_len = bs1[2].len;
				for (i = 0; (i < bs1_2_len) && (*cp2 == *cp1); ++i)
				{
					++cp2;
					++cp1;
				}
				if (i == bs1_2_len)
				{
					cp2 = (unsigned char *)bs1[3].addr;
					bs1_3_len = bs1[3].len;
					for (j = 0; (j < bs1_3_len) && (*cp2 == *cp1); ++j)
					{
						++cp2;
						++cp1;
					}
				}
				n = (int)((sm_long_t)*cp2 - (sm_long_t)*cp1);
				if (0 > n)
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_mkblk;
					GOTO_RETRY;
				} else if (1 < n)
				{
					temp_key->end = cp1 - temp_key->base + 2;
					if (temp_key->end < temp_key->top)
					{
						*cp1++ += (!new_rec_goes_to_right ?  (n - 1) : 1);
						*cp1++ = 0;
						*cp1 = 0;
					} else
					{
						temp_key->end = temp_key->prev;
						assert(temp_key->end < temp_key->top);
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_mkblk;
						GOTO_RETRY;
					}
				} else if (1 == n)
				{
					cp1++;
					start_len = cp1 - temp_key->base + 2;
					if (start_len < temp_key->top)
					{
						if (i == (bs1_2_len - 1))
							cp2 = (unsigned char *)bs1[3].addr;
						else
							cp2++;
						if (((KEY_DELIMITER != *(cp1 - 1)) || (KEY_DELIMITER != *(cp1 - 2)))
						 && ((STR_SUB_MAXVAL != *cp1) || (KEY_DELIMITER != *cp2))
						 && (gv_cur_region->max_key_size > start_len))
						{
							if (!new_rec_goes_to_right)
							{
								old_ch = *cp2;
								new_ch = old_ch - 1;
								*cp1 = new_ch;
								if (KEY_DELIMITER != old_ch)
									*(cp1 - 1) = *(cp2 - 1);
							} else
							{
								old_ch = *cp1;
								new_ch = old_ch + 1;
								*cp1 = new_ch;
								if (STR_SUB_MAXVAL == old_ch)
									*(cp1 - 1) = *(cp2 - 1);
							}
							cp1++;
							if (KEY_DELIMITER == new_ch)
								temp_key->end--;
							else
								*cp1++ = KEY_DELIMITER;
							*cp1 = KEY_DELIMITER;
							temp_key->end = cp1 - temp_key->base;
						}
					} else
					{
						temp_key->end = temp_key->prev;
						assert(temp_key->end < temp_key->top);
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_mkblk;
						GOTO_RETRY;
					}
				}
			}
			assert(temp_key->end < temp_key->top);
			assert(1 <= temp_key->end);
			assert(KEY_DELIMITER == temp_key->base[temp_key->end]);
			assert(KEY_DELIMITER == temp_key->base[temp_key->end - 1]);
			assert((2 > temp_key->end) || (KEY_DELIMITER != temp_key->base[temp_key->end - 2]));
			bq = bh + 1;
			if (HIST_TERMINATOR != bq->blk_num)
			{	/* Not root;  write blocks and continue */
				if (cdb_sc_normal != (status = gvcst_search_blk(temp_key, bq)))
					GOTO_RETRY;
				/* It's necessary to disable the indexmod optimization for splits of index blocks. Refer to
				 * GTM-7353, C9B11-001813 (GTM-3984), and C9H12-002934 (GTM-6104).
				 */
				cse = t_write(bh, (unsigned char *)bs1, ins_chain_offset, ins_chain_index, bh_level,
							TRUE, FALSE, (level_0) ? GDS_WRITE_PLAIN : GDS_WRITE_KILLTN);
				assert(!dollar_tlevel || !cse->high_tlevel);
				if (cse)
				{
					assert(dollar_tlevel);
					cse->write_type |= GDS_WRITE_BLOCK_SPLIT;
				}
				value.len = SIZEOF(block_id);
				value.addr = (char *)&zeroes;
				++bh;
				ins_chain_index = next_blk_index;
			} else
			{	/* Create new root */
				if ((bh_level + 1) == MAX_BT_DEPTH)
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_maxlvl;
					GOTO_RETRY;
				}
				ins_chain_index = t_create(blk_num, (uchar_ptr_t)bs1, ins_chain_offset, ins_chain_index, bh_level);
				make_it_null = FALSE;
				if (NULL != cse)
				{     /* adjust block to use the buffer and offsets worked out for the old root */
					assert(cse->done);
				        assert(NULL != cse->new_buff);
					cse_new = si->last_cw_set;
					assert(!cse_new->high_tlevel);
					cse_new->blk_target = cse->blk_target;
					cse_new->first_off = cse->first_off;
					cse_new->next_off = cse->next_off;
							/* to be able to incrementally rollback, we need another copy of new_buff,
							 * pointer copying wouldn't suffice
							 */
					cse_new->new_buff = (unsigned char *)get_new_free_element(si->new_buff_list);
					memcpy(cse_new->new_buff, cse->new_buff, ((blk_hdr_ptr_t)cse->new_buff)->bsiz);
					cse_new->old_block = NULL;
					make_it_null = TRUE;
				}
				/* Build the right child of the new root right now since it is possible that before commit the
				 * root block may have been recycled in the global buffer which wouldn't cause a restart since
				 * it has been built already (see the gvcst_blk_build below). Otherwise, we may be relying
				 * on incorrect data in the root block when we build this right child finally in bg_update.
				 * Note that this needs to be done only in TP since only tp_tend allows for a block with a
				 * cse not to be in the global buffer if a new_buff already exists.
				 */
				if (dollar_tlevel)
				{
					DEBUG_ONLY(tp_get_cw(si->first_cw_set, ins_chain_index, &cse_new);)
					assert(cse_new == si->last_cw_set);
					cse_new = si->last_cw_set;
					assert(FALSE == cse_new->done);
					assert(!cse_new->high_tlevel);
					gvcst_blk_build(cse_new, NULL, 0);
					cse_new->done = TRUE;
				}
				target_key_size = temp_key->end + 1;
				BLK_INIT(bs_ptr, bs1);
				BLK_ADDR(curr_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
				curr_rec_hdr->rsiz = target_key_size + SIZEOF(rec_hdr) + SIZEOF(block_id);
				SET_CMPC(curr_rec_hdr, 0);
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, SIZEOF(rec_hdr));
				BLK_ADDR(cp1, target_key_size, unsigned char);
				memcpy(cp1, temp_key->base, target_key_size);
				BLK_SEG(bs_ptr, cp1, target_key_size);
				BLK_SEG(bs_ptr, (unsigned char *)&zeroes, SIZEOF(block_id));
				BLK_ADDR(next_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
				next_rec_hdr->rsiz = BSTAR_REC_SIZE;
				SET_CMPC(next_rec_hdr, 0);
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)next_rec_hdr, SIZEOF(rec_hdr));
				BLK_SEG(bs_ptr, (unsigned char *)&zeroes, SIZEOF(block_id));
				if (0 == BLK_FINI(bs_ptr, bs1))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_mkblk;
					GOTO_RETRY;
				}
				assert(bs1[0].len <= blk_reserved_size); /* Assert that new block has space for reserved bytes */
				ins_off1 = (block_offset)(SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + target_key_size);
				ins_off2 = (block_offset)(SIZEOF(blk_hdr) + (2 * SIZEOF(rec_hdr)) + SIZEOF(block_id) +
							  target_key_size);
				assert(ins_off1 < ins_off2);
				/* Since a new root block is not created but two new children are created, this update to the
				 * root block should disable the "indexmod" optimization (C9B11-001813).
				 */
				cse = t_write(bh, (unsigned char *)bs1, ins_off1, next_blk_index,
							bh_level + 1, TRUE, FALSE, GDS_WRITE_KILLTN);
				if (make_it_null)
					cse->new_buff = NULL;
				assert(!dollar_tlevel || !cse->high_tlevel);
				if (!dollar_tlevel)
				{	/* create a sibling cw-set-element to store ins_off2/ins_chain_index */
					t_write_root(ins_off2, ins_chain_index);
				} else
				{
					cse->write_type |= GDS_WRITE_BLOCK_SPLIT;
					assert(NULL == cse->new_buff);
					cse->first_off = 0;
					cse->next_off = ins_off2 - ins_off1;
					/* the following is the only place where the buffer is not completely built by
					 * gvcst_blk_build. this means that the block chain seen by gvcst_blk_build will
					 * have a bad value (that is fixed below) at the end of the list. therefore the
					 * block chain integrity checking code in gvcst_blk_build will error out normally
					 * in this case. signal that routine to skip checking just this tail element.
					 */
					DEBUG_ONLY(skip_block_chain_tail_check = TRUE;)
					gvcst_blk_build(cse, NULL, 0);
					DEBUG_ONLY(skip_block_chain_tail_check = FALSE;)
					curr_chain.flag = 1;
					curr_chain.cw_index = ins_chain_index;
					curr_chain.next_off = 0;
					curr = cse->new_buff + ins_off2;
					GET_LONGP(curr, &curr_chain);
					cse->done = TRUE;
					gv_target->clue.end = 0;
				}
				succeeded = TRUE;
			}
		}
	}
	assert(succeeded);
	horiz_growth = FALSE;
	assert((csa->dir_tree == gv_target) || tp_root);
	RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ, DO_GVT_GVKEY_CHECK);
	/* The only case where gv_target is still csa->dir_tree after the above RESET macro is if op_gvput was invoked
	 * with gv_target being set to cs_addrs->dir_tree. In that case gbl_target_was_set would have been set to TRUE. Assert.
	 */
	assert((csa->dir_tree != gv_target) || gbl_target_was_set);
	/* Format the journal records only once for non-TP (irrespective of number of restarts).
	 * We remember this through the variable "jnl_format_done". If TRUE, we do not redo the jnl_format.
	 * The only exception is if we are in $INCREMENT in which case we need to reformat since the
	 *	current value (and hence the post-increment value) of the key might be different in different tries.
	 *	In this case, the restart code checks and resets "jnl_format_done" to FALSE.
	 */
	if (!dollar_tlevel)
	{
		nodeflags = 0;
		if (skip_dbtriggers)
			nodeflags |= JS_SKIP_TRIGGERS_MASK;
		if (duplicate_set)
			nodeflags |= JS_IS_DUPLICATE;
		assert(!jnl_format_done || !is_dollar_incr && (JNL_SET == non_tp_jfb_ptr->ja.operation));
		if (need_extra_block_split)
                        inctn_opcode = inctn_gvcstput_extra_blk_split;
		else if (JNL_WRITE_LOGICAL_RECS(csa) && !jnl_format_done)
		{
			jfb = jnl_format(JNL_SET, gv_currkey, (!is_dollar_incr ? val_forjnl : post_incr_mval), nodeflags);
			assert(NULL != jfb);
			jnl_format_done = TRUE;
		}
                succeeded = ((trans_num)0 != t_end(&gv_target->hist, dir_hist, TN_NOT_SPECIFIED));
                inctn_opcode = inctn_invalid_op;
		if (succeeded)
		{
			if (NULL != dir_hist)
			{	/* The Global Variable Tree was created in this transaction. So clear its gv_target to be safe.
				 * The directory tree though will have a non-zero value and that can stay as it is since it
				 * was validated in this transaction and was found good enough for us to commit.
				 */
				assert(dir_tree != gv_target);
				gv_target->clue.end = 0;
			}
		} else
		{	/* "t_retry" would have already been invoked by "t_end".
			 * So instead of going to "retry:", do only whatever steps from there are necessary here.
			 */
			RESTORE_ZERO_GVT_ROOT_ON_RETRY(lcl_root, gv_target, dir_hist, dir_tree);
			/*if (is_dollar_incr)*/
				jnl_format_done = FALSE;	/* need to reformat jnl records for $INCR even in case of non-TP */
			GTMTRIG_DBG_ONLY(dbg_trace_array[dbg_num_iters].retry_line = __LINE__);
			goto tn_restart;
		}
	} else
	{
		status = tp_hist(dir_hist);
		if (NULL != dir_hist)
		{	/* Note that although "tp_hist" processes the "dir_hist" history, it only adds "gv_target" to gvt_tp_list.
			 * But csa->dir_tree might have had clue, blk-split related info etc. modified as part of this
			 * gvcst_put invocation that might also need cleanup (just like any other gv_target) so add
			 * csa->dir_tree to gvt_tp_list (if not already done). Therefore treat this as if tp_hist is doing
			 * the ADD_TO_GVT_TP_LIST call for dir_tree.
			 */
			assert(dir_tree == csa->dir_tree);
			ADD_TO_GVT_TP_LIST(dir_tree, RESET_FIRST_TP_SRCH_STATUS_FALSE);
				/* note: above macro updates read_local_tn if necessary */
		}
		if (cdb_sc_normal != status)
			GOTO_RETRY;
		jnl_format_done = !needfmtjnl;
	}
	if (succeeded)
	{
		if (0 == tp_root)
		{	/* Fill in gv_target->root with newly created root block value.
			 * Previously, root remained at 0 at the end of the transaction and it was left to the
			 * NEXT transaction to do a gvcst_root_search and determine the new root block.
			 * This was fine until recently when op_gvrectarg was reworked to NOT do a gvcst_root_search
			 * (to avoid potential TP restarts while unwinding the M stack). This meant that gv_target->root
			 * needed to be kept uptodate as otherwise it was possible for gv_target->root to be stale
			 * after a op_gvrectarg causing incorrect behavior of following M code (see v52000/C9B10001765
			 * subtest for example where $order(^gvn,$$extrinsic) is done and extrinsic CREATES <^gvn>).
			 */
			GTMTRIG_ONLY(assert(!ztval_gvcst_put_redo);)
			assert(0 == gv_target->root);
			if (!dollar_tlevel)
			{
				tp_root = cw_set[root_blk_cw_index].blk;
				assert(gds_t_acquired == cw_set[root_blk_cw_index].old_mode);
				assert(gds_t_committed == cw_set[root_blk_cw_index].mode);
				assert(!IS_BITMAP_BLK(tp_root));
			} else
			{
				chain1.flag = 1;
				chain1.cw_index = root_blk_cw_index;
				chain1.next_off = 0;	/* does not matter what value we set this field to */
				assert(SIZEOF(tp_root) == SIZEOF(chain1));
				tp_root = *(block_id *)&chain1;
			}
			gv_target->root = tp_root;
		}
		if (need_extra_block_split)
		{	/* The logical update required an extra block split operation first (which succeeded) so
			 * get back to doing the logical update before doing any trigger invocations etc.
			 */
			GTMTRIG_ONLY(skip_hasht_read = (dollar_tlevel) ? TRUE : skip_hasht_read;)
			goto fresh_tn_start;
		}
		for (bh_level = 0; bh_level < split_depth; bh_level++)
		{
			blk_num = last_split_blk_num[bh_level];
			assert(0 != blk_num);
			split_targ->last_split_blk_num[bh_level] = blk_num;
			assert((NEWREC_DIR_FORCED == last_split_direction[bh_level])
				|| (NEWREC_DIR_LEFT == last_split_direction[bh_level])
				|| (NEWREC_DIR_RIGHT == last_split_direction[bh_level]));
			split_targ->last_split_direction[bh_level] = last_split_direction[bh_level];
			/* Fix blk_num if it was created in this transaction. In case of non-TP, we have the real block number
			 * corresponding to the created block. In case of TP, we can know that only at tp_clean_up time so defer.
			 */
			chain1 = *(off_chain *)&blk_num;
			if (chain1.flag)
			{
				if (!dollar_tlevel)
				{
					assert(chain1.cw_index < ARRAYSIZE(cw_set));
					split_targ->last_split_blk_num[bh_level] = cw_set[chain1.cw_index].blk;
				} else
					split_targ->split_cleanup_needed = TRUE;/* phantom blk# will be fixed at tp_clean_up time */
			}
		}
		if (dollar_tlevel)
		{
			nodeflags = 0;
			if (skip_dbtriggers)
				nodeflags |= JS_SKIP_TRIGGERS_MASK;
			if (duplicate_set)
				nodeflags |= JS_IS_DUPLICATE;
			ja_val = (!is_dollar_incr ? val_forjnl : post_incr_mval);
			write_logical_jnlrecs = JNL_WRITE_LOGICAL_RECS(csa);
#			ifdef GTM_TRIGGER
			if (!skip_dbtriggers && parms->enable_trigger_read_and_fire)
			{
				/* Since we are about to invoke the trigger, we better have gv_target->gvt_trigger and
				 * the local variable gvt_trigger in sync. The only exception is when we are here because
				 * of a $ztvalue update and redoing the gvcst_put. In this case, it's possible that
				 * the trigger code that was previously executed deleted the trigger and did an update
				 * on the global which would have set gv_target->gvt_trigger to NULL. Assert accordingly.
				 */
				assert(ztval_gvcst_put_redo || (gvt_trigger == gv_target->gvt_trigger));
				if ((NULL != gvt_trigger) && !ztval_gvcst_put_redo)
				{
					assert(dollar_tlevel);
					/* Format ZTWORM and SET journal records.
					 * "ztworm_jfb", "jfb" and "jnl_format_done" are set by the below macro.
					 */
					JNL_FORMAT_ZTWORM_IF_NEEDED(csa, write_logical_jnlrecs,
							JNL_SET, gv_currkey, ja_val, ztworm_jfb, jfb, jnl_format_done);
					/* Initialize trigger parms that dont depend on the context of the matching trigger */
					trigparms.ztoldval_new = key_exists ? ztold_mval : (mval *)&literal_null;
					PUSH_MV_STENT(MVST_MVAL);	/* protect $ztval from stp_gcol */
					ztval_mval = &mv_chain->mv_st_cont.mvs_mval;
					if (!is_dollar_incr)
						*ztval_mval = *val_forjnl;
					else
					{
						*ztval_mval = *post_incr_mval;
						/* Since this is pointing to malloced buffer, we need to repoint it to stringpool
						 * to avoid a nested trigger call (that does a $INCR) from overwriting this buffer.
						 * This way buffers corresponding to $ztvals of nested triggers can coexist.
						 */
						s2pool(&ztval_mval->str);
					}
					trigparms.ztvalue_new = ztval_mval;
					trigparms.ztdata_new = key_exists ? &literal_one : &literal_zero;
					gvtr_parms.gvtr_cmd = GVTR_CMDTYPE_SET;
					gvtr_parms.gvt_trigger = gvt_trigger;
					/* Now that we have filled in minimal information, let "gvtr_match_n_invoke" do the rest */
					gtm_trig_status = gvtr_match_n_invoke(&trigparms, &gvtr_parms);
					assert((0 == gtm_trig_status) || (ERR_TPRETRY == gtm_trig_status));
					if (ERR_TPRETRY == gtm_trig_status)
					{	/* A restart has been signaled that we need to handle or complete the handling of.
						 * This restart could have occurred reading the trigger in which case no
						 * tp_restart() has yet been done or it could have occurred in trigger code in
						 * which case we need to finish the incomplete tp_restart. In both cases this
						 * must be an implicitly TP wrapped transaction. Our action is to complete the
						 * necessary tp_restart() logic (t_retry is already completed so should be skipped)
						 * and then re-do the gvcst_put logic.
						 */
						assert(lcl_implicit_tstart || lcl_span_status);
						assert(CDB_STAGNATE >= t_tries);
						status = cdb_sc_normal;	/* signal "retry:" to avoid t_retry call */
						GOTO_RETRY;
					}
					REMOVE_ZTWORM_JFB_IF_NEEDED(ztworm_jfb, jfb, si);
					if (trigparms.ztvalue_changed)
					{	/* At least one of the invoked triggers changed $ztval.
						 * Redo the gvcst_put with $ztval as the right side of the SET.
						 * Also make sure gtm_trigger calls are NOT done this time around.
						 */
						assert(0 < gvtr_parms.num_triggers_invoked);
						val = trigparms.ztvalue_new;
						val_forjnl = trigparms.ztvalue_new;
						MV_FORCE_STR(val); /* in case the updated value happens to be a numeric quantity */
						fits = RECORD_FITS_IN_A_BLOCK(val, gv_currkey, blk_size, blk_reserved_bytes);
						if (!fits)
						{	/* If val is now too big to fit in a block, we need to back out into
							 * gvcst_put and try again with spanning nodes. This means we should
							 * OP_TROLLBACK if lcl_implicit_tstart.
							 */
							if (lcl_implicit_tstart)
							{
								POP_MVALS_FROM_M_STACK_IF_NEEDED(ztold_mval, save_msp,
										save_mv_chain);
								OP_TROLLBACK(-1);
								assert(!lcl_span_status);
								parms->span_status = TRUE;
							} else
							{
								parms->ztval_gvcst_put_redo = TRUE;
								parms->ztval_mval = val;
							}
							RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ, DO_GVT_GVKEY_CHECK);
							return;
						}
						ztval_gvcst_put_redo = TRUE;
						skip_hasht_read = TRUE;
						/* In case, the current gvcst_put invocation was for $INCR, reset the corresponding
						 * global variable that indicates a $INCR is in progress since the redo of the
						 * gvcst_put is a SET command (no longer $INCR).
						 */
						is_dollar_incr = FALSE;
						/* Dont pop the mvals as we want ztval_mval (which points to the mval containing
						 * "val" for the redo iteration) protected-from-stp_gcol/accessible until the
						 * redo is complete.
						 */
						goto fresh_tn_start;
					}
				}
				/* We don't want to pop mvals yet if we still need to set chunks of ztval */
				POP_MVALS_FROM_M_STACK_IF_REALLY_NEEDED(lcl_span_status, ztold_mval, save_msp, save_mv_chain);
					/* pop any stacked mvals before op_tcommit as it does its own popping */
			}
#			endif
			if (write_logical_jnlrecs && !jnl_format_done)
			{
				assert(dollar_tlevel);
#				ifdef GTM_TRIGGER
				/* Do not replicate implicit update or $ztval redo update */
				assert(tstart_trigger_depth <= gtm_trigger_depth);
				if ((gtm_trigger_depth > tstart_trigger_depth) || ztval_gvcst_put_redo)
				{
					/* Ensure that JS_SKIP_TRIGGERS_MASK and JS_NOT_REPLICATED_MASK are mutually exclusive. */
					assert(!(nodeflags & JS_SKIP_TRIGGERS_MASK));
					nodeflags |= JS_NOT_REPLICATED_MASK;
				}
#				endif
				jfb = jnl_format(JNL_SET, gv_currkey, ja_val, nodeflags);
				assert(NULL != jfb);
				jnl_format_done = TRUE;
			}
#			ifdef GTM_TRIGGER
			/* Go ahead with commit of any implicit TP wrapped transaction */
			if (lcl_implicit_tstart)
			{
				GVTR_OP_TCOMMIT(status);
				if (cdb_sc_normal != status)
					GOTO_RETRY;
			}
#			endif
		}
		assert(!JNL_WRITE_LOGICAL_RECS(csa) || jnl_format_done);
		/* Now that the SET/$INCR is finally complete, increment the corresponding GVSTAT counter */
		if (!lcl_span_status)
			INCR_GVSTATS_COUNTER(csa, cnl, n_set, 1);
		DBG_CHECK_VAL_AT_FUN_EXIT;
		assert(lcl_dollar_tlevel == dollar_tlevel);
		return;
	}
retry:
	/* Note that it is possible cs_addrs is not equal to csa at this point in case we restarted due to trigger
	 * invocations and in case those triggers referenced globals in different regions. But this should be fixed
	 * by a call to t_retry/tp_restart below (it does a TP_CHANGE_REG(tp_pointer->gd_reg)).
	 */
#	ifdef GTM_TRIGGER
	if (lcl_implicit_tstart)
	{
		assert(!skip_dbtriggers);
		assert(!skip_INVOKE_RESTART);
		assert((cdb_sc_normal != status) || (ERR_TPRETRY == gtm_trig_status));
		if (cdb_sc_normal != status)
			skip_INVOKE_RESTART = TRUE; /* causes t_retry to invoke only tp_restart without any rts_error */
		/* else: t_retry has already been done by gtm_trigger so no need to do it again for this try */
		/* If an implicitly TP wrapped transaction is restarting, restore things to what they were
		 * at entry into gvcst_put. Note that we could have done multiple iterations of gvcst_put for
		 * extra_block_split/retry/ztval_gvcst_put_redo.
		 */
		ztval_gvcst_put_redo = FALSE;
		skip_hasht_read = FALSE;
		val = lcl_val;
		val_forjnl = lcl_val_forjnl;
		/* $increment related fields need to be restored */
		is_dollar_incr = lcl_is_dollar_incr;
		post_incr_mval = lcl_post_incr_mval;
		increment_delta_mval = lcl_increment_delta_mval;
	}
#	endif
	assert((cdb_sc_normal != status) GTMTRIG_ONLY(|| lcl_implicit_tstart || lcl_span_status));
	if (cdb_sc_normal != status)
	{	/* Need to restart. If directory tree was used in this transaction, nullify its clue as well (not normally
		 * done by t_retry). The RESTORE_ZERO_GVT_ROOT_ON_RETRY macro call below takes care of that for us.
		 */
		RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ, dollar_tlevel ? DO_GVT_GVKEY_CHECK_RESTART : DO_GVT_GVKEY_CHECK);
		RESTORE_ZERO_GVT_ROOT_ON_RETRY(lcl_root, gv_target, dir_hist, dir_tree);
		GTMTRIG_ONLY(POP_MVALS_FROM_M_STACK_IF_REALLY_NEEDED(lcl_span_status, ztold_mval, save_msp, save_mv_chain));
		t_retry(status);
		GTMTRIG_ONLY(skip_INVOKE_RESTART = FALSE);
	} else
	{	/* else: t_retry has already been done so no need to do that again but need to still invoke tp_restart
		 * to complete pending "tprestart_state" related work.
		 * SKIP_GVT_GVKEY_CHECK allows us to skip DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC if a trigger invocation
		 * restarted during GVCST_ROOT_SEARCH, in which case gv_currkey will correspond to ^#t.
		 * The subsequent tp_restart restores gv_currkey/gv_target to an in-sync state.
		 */
		RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ, SKIP_GVT_GVKEY_CHECK);
#		ifdef GTM_TRIGGER
		assert(ERR_TPRETRY == gtm_trig_status);
		TRIGGER_BASE_FRAME_UNWIND_IF_NOMANSLAND;
		POP_MVALS_FROM_M_STACK_IF_REALLY_NEEDED(lcl_span_status, ztold_mval, save_msp, save_mv_chain)
		if (!lcl_implicit_tstart)
		{	/* We started an implicit transaction for spanning nodes in gvcst_put. Invoke restart to return. */
			assert(lcl_span_status && !skip_INVOKE_RESTART && (&gvcst_put_ch == ctxt->ch));
			INVOKE_RESTART;
		}
#		endif
		rc = tp_restart(1, !TP_RESTART_HANDLES_ERRORS);
		assert(0 == rc GTMTRIG_ONLY(&& TPRESTART_STATE_NORMAL == tprestart_state));
		DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE); /* finishs the check skipped above */
		RESTORE_ZERO_GVT_ROOT_ON_RETRY(lcl_root, gv_target, dir_hist, dir_tree);
	}
#	ifdef GTM_TRIGGER
	assert(0 < t_tries);
	assert((cdb_sc_normal != status) || (ERR_TPRETRY == gtm_trig_status));
	if (cdb_sc_normal == status)
	{
		DEBUG_ONLY(save_cdb_status = status);
		status = LAST_RESTART_CODE;
	}
	assert((cdb_sc_onln_rlbk2 != status) || TREF(dollar_zonlnrlbk));
	assert(((cdb_sc_onln_rlbk1 != status) && (cdb_sc_onln_rlbk2 != status)) || !gv_target->root);
	if ((cdb_sc_onln_rlbk2 == status) && lcl_implicit_tstart)
	{	/* Database was taken back to a different logical state and we are an implicit TP transaction.
		 * Issue DBROLLEDBACK error that the application programmer can catch and do the necessary stuff.
		 */
		assert(gtm_trigger_depth == tstart_trigger_depth);
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_DBROLLEDBACK);
	}
	/* Note: In case of cdb_sc_onln_rlbk1, the restart logic will take care of doing the root search */
#	endif
	GTMTRIG_ONLY(assert(!skip_INVOKE_RESTART);) /* if set to TRUE a few statements above, should have been reset by t_retry */
	/* At this point, we can be in TP only if we implicitly did a tstart in gvcst_put (as part of a trigger update).
	 * Assert that. Since the t_retry/tp_restart would have reset si->update_trans, we need to set it again.
	 * So reinvoke the T_BEGIN call only in case of TP. For non-TP, update_trans is unaffected by t_retry.
	 */
	assert(!dollar_tlevel GTMTRIG_ONLY(|| lcl_implicit_tstart));
	if (dollar_tlevel)
	{
		jnl_format_done = !needfmtjnl;	/* need to reformat jnl records unconditionally in case of TP */
		tp_set_sgm();	/* set sgm_info_ptr & first_sgm_info for TP start */
		T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_GVPUTFAIL);	/* set update_trans and t_err for wrapped TP */
	} else if (is_dollar_incr)
		jnl_format_done = !needfmtjnl;	/* need to reformat jnl records for $INCR even in case of non-TP */
	assert(dollar_tlevel || update_trans);
	goto tn_restart;
}
