/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

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
#ifdef UNIX
#include "error.h"
#endif

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
#ifdef UNIX
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
#endif
#ifdef GTM_TRIGGER
GBLREF	boolean_t		skip_INVOKE_RESTART;
#endif

error_def(ERR_GVGETFAIL);

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

#ifdef UNIX
#define SAVE_ROOTSRCH_ENTRY_STATE								\
{												\
	int				idx;							\
	redo_root_search_context	*rootsrch_ctxt_ptr;					\
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
	inctn_opcode = 0;									\
	rootsrch_ctxt_ptr->t_err = t_err;							\
	rootsrch_ctxt_ptr->hold_onto_crit = cs_addrs->hold_onto_crit;				\
	if (CDB_STAGNATE <= t_tries)								\
	{											\
		assert(cs_addrs->now_crit);							\
		cs_addrs->hold_onto_crit = TRUE;						\
	}											\
	if (mu_reorg_process)									\
	{	/* In case gv_currkey/gv_target are out of sync. */				\
		rootsrch_ctxt_ptr->gv_currkey = (gv_key *)&rootsrch_ctxt_ptr->currkey[0];	\
		MEMCPY_KEY(rootsrch_ctxt_ptr->gv_currkey, gv_currkey);				\
		SET_GV_CURRKEY_FROM_REORG_GV_TARGET;						\
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
	START_CH;

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
#endif

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
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((dba_bg == gv_cur_region->dyn.addr->acc_meth) || (dba_mm == gv_cur_region->dyn.addr->acc_meth));
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
	save_targ->root = lcl_root;	/* now that we know the transaction validated fine, set root block in gv_target */
	RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ, DO_GVT_GVKEY_CHECK);
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
	} else
	{
		gv_target->nct = 0;
		gv_target->act = cs_addrs->hdr->def_coll;
		gv_target->ver = cs_addrs->hdr->def_coll_ver;
	}
	if (gv_target->act)
		act_in_gvt();
	assert(gv_target->act || NULL == gv_target->collseq);
	return cdb_sc_normal;
}
