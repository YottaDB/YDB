/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "t_begin.h"
#include "have_crit.h"
#include "db_snapshot.h"
#ifdef DEBUG
#include "tp_frame.h"
#endif

GBLREF	boolean_t		mu_reorg_process, need_kip_incr;
GBLREF	cw_set_element		cw_set[];
GBLREF	gv_namehead		*gv_target;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	sgm_info		*first_sgm_info;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	short			crash_count;
GBLREF	trans_num		start_tn;
GBLREF	uint4			dollar_tlevel, mu_upgrade_in_prog, t_err, update_trans;
GBLREF	unsigned char		cw_set_depth, cw_map_depth;
GBLREF	unsigned int		t_tries;
GBLREF	volatile int4		fast_lock_count;
#ifdef DEBUG
GBLREF	sgmnt_addrs		*reorg_encrypt_restart_csa;
GBLREF	uint4			bml_save_dollar_tlevel;
#endif

error_def(ERR_MMREGNOACCESS);

void t_begin(uint4 err, uint4 upd_trans) 	/* err --> error code for current gvcst_routine */
{
	sgmnt_addrs		*csa;
	srch_blk_status		*s;
	trans_num		histtn;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!dollar_tlevel); /* if in TP, the T_BEGIN_xxx_NONTP_OR_TP macro should have been used and we will not be here */
	assert((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) || (mu_reorg_process && (INTRPT_IN_KILL_CLEANUP == intrpt_ok_state)));
	assert(NULL == reorg_encrypt_restart_csa);
	/* The wcs_wtstart and dsk_read functions rely on the update_trans global to detect whether they are inside a read-write
	 * non-TP transaction, in which case they may trigger a restart if encryption settings have been concurrently modified by
	 * MUPIP REORG -ENCRYPT. We verify below that update_trans is not set prior to a transaction, as otherwise it would be
	 * possible to attempt a transaction restart inside wcs_wtstart or dsk_read (by doing, say, VIEW "FLUSH") while not being in
	 * a transaction.
	 */
	assert(!update_trans);
	/* The update_trans global should not be set at the start of the transaction because otherwise it would cause
	 * csd->kill_in_prog to be incorrectly incremented for the current transaction.
	 */
	assert(!upd_trans || !need_kip_incr);
	/* Any changes to the initialization in the two lines below might need a similar change in T_BEGIN_xxx_NONTP_OR_TP macros */
	update_trans = upd_trans;
	t_err = err;
	csa = cs_addrs;
	if ((NULL == csa->db_addrs[0]) && (dba_mm == csa->hdr->acc_meth))
	{
		RTS_ERROR_CSA_ABT(csa, VARLSTCNT(6) ERR_MMREGNOACCESS, 4, REG_LEN_STR(csa->region),
			DB_LEN_STR(csa->region));
	}
	/* If we use a clue then we must consider the oldest tn in the search history to be the start tn for this transaction */
        /* start_tn manipulation for TP taken care of in tp_hist */
	UPDATE_CRASH_COUNT(csa, crash_count);
	start_tn = csa->ti->curr_tn;
	/* Note: If gv_target was NULL before the start of a transaction and the only operations done inside the transaction
	 * are trigger deletions causing bitmap free operations, we can reach here with gv_target being NULL.
	 */
	if ((NULL != gv_target) && gv_target->clue.end)
	{	/* Since we have a clue, determine if the clue history has lesser transaction numbers and if so use that
		 * as the start tn. Note that we need to take the MIN of all history level tns (see comment in tp_tend.c
		 * in valid_thru processing logic for why).
		 */
		for (s = &gv_target->hist.h[0]; s->blk_num; s++)
		{
			histtn = s->tn;
			/* Assert that we have a NULL cse in case of a non-zero clue as this will otherwise confuse t_end.c.
			 * The only exceptions are
			 *	a) reorg in which case we nullify the clue AFTER the t_begin call (in mu_reorg.c, mu_swap_root.c,
			 *	   mu_truncate.c) but BEFORE the gvcst_search call so the clue does not get used.
			 *	b) gvcst_bmp_mark_free : It could be invoked from op_tcommit (through gvcst_expand_free_subtree)
			 *	   to free up blocks in a bitmap in which case it does not deal with histories/gv_target and
			 *	   so gv_target->clue and/or gv_target->hist.h[x].cse does not matter. The global variable
			 *	   bml_save_dollar_tlevel exactly identifies this scenario.
			 */
			assert(mu_reorg_process || mu_upgrade_in_prog || (NULL == s->cse) || bml_save_dollar_tlevel);
			if (start_tn > histtn)
				start_tn = histtn;
		}
	}
	cw_set_depth = 0;
	cw_map_depth = 0;
	/* since this is mainline code and we know fast_lock_count should be 0 at this point reset it just in case it is not.
	 * having fast_lock_count non-zero will defer the database flushing logic and other critical parts of the system.
	 * hence this periodic reset at the beginning of each transaction.
	 */
	assert((0 == fast_lock_count) || process_exiting);
	fast_lock_count = 0;
	t_tries = 0;
	if (non_tp_jfb_ptr)
		non_tp_jfb_ptr->record_size = 0; /* re-initialize it to 0 since TOTAL_NONTPJNL_REC_SIZE macro uses it */
	assert(!TREF(donot_commit));
	assert(!csa->now_crit || csa->hold_onto_crit); /* shouldn't hold crit at begin of transaction unless asked to */
	/* Begin of a fresh transaction. This is analogous to tp_set_sgm done for TP transactions. However, unlike tp_set_sgm,
	 * we do not sync trigger cycles here. The reasoning is that trigger load events are in-frequent in the field and since
	 * Non-TP restarts are not as heavy-weight as TP restart, we chose to avoid unconditional memory references here and instead
	 * take the hit of infrequent restarts due to concurrent trigger loads
	 */
}
