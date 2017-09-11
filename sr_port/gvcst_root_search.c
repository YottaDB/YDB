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

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gtmio.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cdb_sc.h"
#include "copy.h"
#include "spec_type.h"
#include "stringpool.h"
#include "filestruct.h"		/* needed for jnl.h */
#include "gdscc.h"		/* needed for tp.h */
#include "jnl.h"		/* needed for tp.h */
#include "gdskill.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"			/* needed for T_BEGIN_READ_NONTP_OR_TP macro */

#include "t_end.h"		/* prototypes */
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_protos.h"	/* for gvcst_search,gvcst_root_search prototype */
#include "get_spec.h"
#include "collseq.h"
#include "gtmimagename.h"
#include "error.h"
#include "io.h"

GBLREF	gv_key		*gv_currkey, *gv_altkey;
GBLREF	int4		gv_keysize;
GBLREF	gv_namehead	*gv_target;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	gd_region	*gv_cur_region;
GBLREF	uint4		dollar_tlevel;
GBLREF	uint4		dollar_trestart;
GBLREF	unsigned int	t_tries;
GBLREF	gv_namehead	*reset_gv_target;
GBLREF	boolean_t	mu_reorg_process;
GBLREF	boolean_t	mupip_jnl_recover;
# ifdef DEBUG
GBLREF	boolean_t	is_rcvr_server;
GBLREF	boolean_t	is_src_server;
GBLDEF	unsigned char	t_fail_hist_dbg[T_FAIL_HIST_DBG_SIZE];
GBLDEF	unsigned int	t_tries_dbg;
# endif
GBLREF	jnl_gbls_t	jgbl;
GBLREF	unsigned char	t_fail_hist[CDB_MAX_TRIES];
GBLREF	trans_num	start_tn;
GBLREF	uint4		update_trans;
GBLREF	inctn_opcode_t	inctn_opcode;
GBLREF	uint4		t_err;
#ifdef GTM_TRIGGER
GBLREF	boolean_t		skip_INVOKE_RESTART;
#endif

error_def(ERR_ACTCOLLMISMTCH);
error_def(ERR_GVGETFAIL);
error_def(ERR_NCTCOLLSPGBL);

static	mstr	global_collation_mstr;

#ifdef GTM_TRIGGER
# define TRIG_TP_SET_SGM					\
{								\
	if (dollar_tlevel)					\
	{							\
		assert(skip_INVOKE_RESTART);			\
		tp_set_sgm();					\
	}							\
}
#else
# define TRIG_TP_SET_SGM
#endif

#define T_RETRY_AND_CLEANUP(STATUS, DONOT_RESTART)						\
{												\
	gv_target->clue.end = 0;								\
	RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ, DO_GVT_GVKEY_CHECK);				\
	if (DONOT_RESTART)									\
		return status; /* caller will handle the restart */				\
	t_retry(STATUS);									\
	save_targ->root = 0;	/* May have been found by gvcst_redo_root_search.		\
				 * Reset and allow gvcst_root_search to find it itself.		\
				 */								\
	TRIG_TP_SET_SGM;									\
}

#define SAVE_ROOTSRCH_ENTRY_STATE								\
{												\
	int				idx;							\
	redo_root_search_context	*rootsrch_ctxt_ptr;					\
												\
	GBLREF	gv_namehead		*reorg_gv_target;					\
												\
	rootsrch_ctxt_ptr = &(TREF(redo_rootsrch_ctxt));					\
	rootsrch_ctxt_ptr->t_tries = t_tries;							\
	for (idx = 0; CDB_MAX_TRIES > idx; idx++)						\
		rootsrch_ctxt_ptr->t_fail_hist[idx] = t_fail_hist[idx];				\
	rootsrch_ctxt_ptr->prev_t_tries = TREF(prev_t_tries);					\
	DEBUG_ONLY(										\
		rootsrch_ctxt_ptr->t_tries_dbg = t_tries_dbg;					\
		for (idx = 0; T_FAIL_HIST_DBG_SIZE > idx; idx++)				\
			rootsrch_ctxt_ptr->t_fail_hist_dbg[idx] = t_fail_hist_dbg[idx];		\
	)											\
	rootsrch_ctxt_ptr->start_tn = start_tn;							\
	rootsrch_ctxt_ptr->update_trans = update_trans;						\
	rootsrch_ctxt_ptr->inctn_opcode = inctn_opcode;						\
	/* Resetting and restoring of update_trans is necessary to avoid blowing an assert in	\
	 * t_begin that it is 0.								\
	 */											\
	update_trans = 0;									\
	inctn_opcode = inctn_invalid_op;							\
	rootsrch_ctxt_ptr->t_err = t_err;							\
	rootsrch_ctxt_ptr->hold_onto_crit = cs_addrs->hold_onto_crit;				\
	if (CDB_STAGNATE <= t_tries)								\
	{											\
		assert(cs_addrs->now_crit);							\
		cs_addrs->hold_onto_crit = TRUE;						\
	}											\
	if (mu_reorg_process)									\
	{	/* In case gv_currkey/gv_target are out of sync. */				\
		rootsrch_ctxt_ptr->gv_currkey = &rootsrch_ctxt_ptr->currkey[0];			\
		MEMCPY_KEY(rootsrch_ctxt_ptr->gv_currkey, gv_currkey);				\
		SET_GV_CURRKEY_FROM_GVT(reorg_gv_target);					\
	}											\
}

#define RESTORE_ROOTSRCH_ENTRY_STATE								\
{												\
	int				idx;							\
	redo_root_search_context	*rootsrch_ctxt_ptr;					\
												\
	rootsrch_ctxt_ptr = &(TREF(redo_rootsrch_ctxt));					\
	t_tries = rootsrch_ctxt_ptr->t_tries;							\
	for (idx = 0; CDB_MAX_TRIES > idx; idx++)						\
		t_fail_hist[idx] = rootsrch_ctxt_ptr->t_fail_hist[idx];				\
	TREF(prev_t_tries) = rootsrch_ctxt_ptr->prev_t_tries;					\
	DEBUG_ONLY(										\
		t_tries_dbg = rootsrch_ctxt_ptr->t_tries_dbg;					\
		for (idx = 0; T_FAIL_HIST_DBG_SIZE > idx; idx++)				\
			t_fail_hist_dbg[idx] = rootsrch_ctxt_ptr->t_fail_hist_dbg[idx];		\
	)											\
	start_tn = rootsrch_ctxt_ptr->start_tn;							\
	update_trans = rootsrch_ctxt_ptr->update_trans;						\
	inctn_opcode = rootsrch_ctxt_ptr->inctn_opcode;						\
	t_err = rootsrch_ctxt_ptr->t_err;							\
	cs_addrs->hold_onto_crit = rootsrch_ctxt_ptr->hold_onto_crit;				\
	TREF(in_gvcst_redo_root_search) = FALSE;						\
	if (mu_reorg_process)									\
		/* Restore gv_currkey */							\
		MEMCPY_KEY(gv_currkey, rootsrch_ctxt_ptr->gv_currkey);				\
}

CONDITION_HANDLER(gvcst_redo_root_search_ch)
{
	START_CH(TRUE);

	RESTORE_ROOTSRCH_ENTRY_STATE;

	NEXTCH;
}

void gvcst_redo_root_search()
{
	DEBUG_ONLY(boolean_t	dbg_now_crit;)
	uint4			lcl_onln_rlbkd_cycle;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH(gvcst_redo_root_search_ch);
	assert(!TREF(in_gvcst_redo_root_search)); /* Should never recurse. However, can be called from non-redo gvcst_root_search,
						   * e.g. from op_gvname. In that case, the results of gvcst_redo_root_search are
						   * discarded and the outer root search correctly sets gv_target->root.
						   */
	TREF(in_gvcst_redo_root_search) = TRUE;
	assert(0 < t_tries);
	assert(!is_src_server && !is_rcvr_server);
	assert(!jgbl.onlnrlbk);
	assert((NULL != gv_target) && !gv_target->root);
	assert(cs_addrs == gv_target->gd_csa);
	assert(!dollar_tlevel);
	DEBUG_ONLY(dbg_now_crit = cs_addrs->now_crit);
	/* save global variables now that we are going to do the root search in the middle of the current transaction */
	SAVE_ROOTSRCH_ENTRY_STATE;
	lcl_onln_rlbkd_cycle = cs_addrs->db_onln_rlbkd_cycle;
	GVCST_ROOT_SEARCH;
	if (lcl_onln_rlbkd_cycle != cs_addrs->nl->db_onln_rlbkd_cycle)
		TREF(rlbk_during_redo_root) = TRUE;
	assert(cs_addrs->now_crit == dbg_now_crit); /* ensure crit state remains same AFTER gvcst_root_search */
	/* restore global variables now that we are continuing with the original transaction */
	RESTORE_ROOTSRCH_ENTRY_STATE;
	REVERT;
}

enum cdb_sc gvcst_root_search(boolean_t donot_restart)
{
	srch_blk_status	*h0;
	sm_uc_ptr_t	rp;
	unsigned short	rlen, hdr_len;
	uchar_ptr_t	subrec_ptr;
	enum cdb_sc	status;
	boolean_t	gbl_target_was_set;
	gv_namehead	*save_targ;
	mname_entry	*gvent;
	int		altkeylen;
	int		tmp_cmpc;
	block_id	lcl_root;
	uint4		oldact, newact, oldnct, oldver;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(IS_REG_BG_OR_MM(gv_cur_region));
	SET_GV_ALTKEY_TO_GBLNAME_FROM_GV_CURRKEY;	/* set up gv_altkey to be just the gblname */
	save_targ = gv_target;
	/* Check if "gv_target->gvname" matches "gv_altkey->base". If not, there is a name mismatch (out-of-design situation).
	 * This check is temporary until we catch the situation that caused D9H02-002641.
	 * It's suspected the original situation has been fixed (see D9I08-002695). But the assertpro will remain until
	 * gvcst_redo_root_search has been well-tested.
	 */
	/* --- Check BEGIN --- */
	gvent = &save_targ->gvname;
	altkeylen = gv_altkey->end - 1;
	assertpro(altkeylen && (altkeylen == gvent->var_name.len)
			&& (0 == memcmp(gv_altkey->base, gvent->var_name.addr, gvent->var_name.len)));
	/* --- Check END   --- */
	if (INVALID_GV_TARGET != reset_gv_target)
		gbl_target_was_set = TRUE;
	else
	{
		gbl_target_was_set = FALSE;
		reset_gv_target = save_targ;
	}
	gv_target = cs_addrs->dir_tree;
	T_BEGIN_READ_NONTP_OR_TP(ERR_GVGETFAIL);
	/* We better hold crit in the final retry (TP & non-TP). Only exception is journal recovery */
	assert((t_tries < CDB_STAGNATE) || cs_addrs->now_crit || mupip_jnl_recover);
	for (;;)
	{
		lcl_root = 0; /* set lcl_root to 0 at the start of every iteration (this way even retry will get fresh value) */
		hdr_len = rlen = 0;
		gv_target = cs_addrs->dir_tree;
		if (dollar_trestart)
			gv_target->clue.end = 0;
		assert(0 == save_targ->root);
		if (cdb_sc_normal == (status = gvcst_search(gv_altkey, 0)))
		{
			if (gv_altkey->end + 1 == gv_target->hist.h[0].curr_rec.match)
			{
				h0 = gv_target->hist.h;
				rp = (h0->buffaddr + h0->curr_rec.offset);
				hdr_len = SIZEOF(rec_hdr) + gv_altkey->end + 1 - EVAL_CMPC((rec_hdr_ptr_t)rp);
				GET_USHORT(rlen, rp);
				if (FALSE == (CHKRECLEN(rp, h0->buffaddr, rlen)) || (rlen < hdr_len + SIZEOF(block_id)))
				{
					T_RETRY_AND_CLEANUP(cdb_sc_rmisalign, donot_restart);
					continue;
				}
				GET_LONG(lcl_root, (rp + hdr_len));
				if (rlen > hdr_len + SIZEOF(block_id))
				{
					assert(NULL != global_collation_mstr.addr || 0 == global_collation_mstr.len);
					if (global_collation_mstr.len < rlen - (hdr_len + SIZEOF(block_id)))
					{
						if (NULL != global_collation_mstr.addr)
							free(global_collation_mstr.addr);
						global_collation_mstr.len = rlen - (hdr_len + SIZEOF(block_id));
						global_collation_mstr.addr = (char *)malloc(global_collation_mstr.len);
					}
					/* the memcpy needs to be done here instead of out of for loop for
					 * concurrency consideration. We don't use s2pool because the pointer rp is 64 bits
					 */
					memcpy(global_collation_mstr.addr, rp + hdr_len + SIZEOF(block_id),
							rlen - (hdr_len + SIZEOF(block_id)));
				}
				if (dollar_tlevel)
				{
					status = tp_hist(NULL);
					if (cdb_sc_normal != status)
					{
						T_RETRY_AND_CLEANUP(status, donot_restart);
						continue;
					}
					break;
				}
			}
			if (!dollar_tlevel)
			{
				if ((trans_num)0 != t_end(&gv_target->hist, NULL, TN_NOT_SPECIFIED))
					break;
			} else
			{
				status = tp_hist(NULL);
				if (cdb_sc_normal == status)
					break;
				T_RETRY_AND_CLEANUP(status, donot_restart);
				continue;
			}
		} else
		{
			T_RETRY_AND_CLEANUP(status, donot_restart);
			continue;
		}
	}
	RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ, DO_GVT_GVKEY_CHECK);
	if (lcl_root)
	{
		oldact = gv_target->act;
		oldnct = gv_target->nct;
		if (rlen > hdr_len + SIZEOF(block_id))
		{
			assert(NULL != global_collation_mstr.addr);
			subrec_ptr = get_spec((uchar_ptr_t)global_collation_mstr.addr,
					      (int)(rlen - (hdr_len + SIZEOF(block_id))), COLL_SPEC);
			if (subrec_ptr)
			{
				gv_target->nct = *(subrec_ptr + COLL_NCT_OFFSET);
				gv_target->act = *(subrec_ptr + COLL_ACT_OFFSET);
				gv_target->ver = *(subrec_ptr + COLL_VER_OFFSET);
			} else
			{
				gv_target->nct = 0;
				gv_target->act = 0;
				gv_target->ver = 0;
			}
		} else if (gv_target->act_specified_in_gld)
		{	/* Global directory specified a collation. Directory tree did not specify any non-zero collation.
			 * So global directory prevails.
			 */
			gv_target->nct = 0;
			/* gv_target->act and gv_target->ver would already have been set in COPY_ACT_FROM_GVNH_REG_TO_GVT macro */
		} else
		{	/* Global directory did not specify a collation. In that case, db file header defaults prevail. */
			gv_target->nct = 0;
			gv_target->act = cs_addrs->hdr->def_coll;
			gv_target->ver = cs_addrs->hdr->def_coll_ver;
		}
		/* If DSE, a runtime global directory is created and hence $gtmgbldir is not as effective as it is for GT.M.
		 * Hence exclude DSE from the below errors which are related to $gtmgbldir.
		 */
		if (!IS_DSE_IMAGE)
		{
			if (gv_target->nct && gv_target->nct_must_be_zero)
			{	/* restore gv_target->act and gv_target->nct */
				gv_target->act = oldact;
				gv_target->nct = oldnct;
				rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_NCTCOLLSPGBL, 4, DB_LEN_STR(gv_cur_region),
						gv_target->gvname.var_name.len, gv_target->gvname.var_name.addr);
			}
			if (gv_target->act_specified_in_gld && (oldact != gv_target->act))
			{
				newact = gv_target->act;
				gv_target->act = oldact;
				gv_target->nct = oldnct;
				rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_ACTCOLLMISMTCH, 6,
						gv_target->gvname.var_name.len, gv_target->gvname.var_name.addr,
						oldact, DB_LEN_STR(gv_cur_region), newact);
			}
		}
	} else if (!gv_target->act_specified_in_gld)
	{	/* If GLD did NOT specify an alternative collation sequence for this global name
		 * but the db file header has a default collation defined, use it.
		 */
		gv_target->nct = 0;
		assert(ACT_NOT_SPECIFIED != cs_addrs->hdr->def_coll);
		gv_target->act = cs_addrs->hdr->def_coll;
		gv_target->ver = cs_addrs->hdr->def_coll_ver;
	}
	if (gv_target->act)
		act_in_gvt(gv_target); /* note: this could issue COLLTYPVERSION error */
	gv_target->root = lcl_root;	/* now that we know the transaction validated fine, set root block in gv_target */
	assert(gv_target->act || NULL == gv_target->collseq);
	return cdb_sc_normal;
}
