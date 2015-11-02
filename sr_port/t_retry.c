/****************************************************************
 *								*
 *	Copyright 2001, 2002  Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "cdb_sc.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "gdsbml.h"
#include "ccp.h"
#include "error.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h and cws_insert.h */
#include "tp.h"
#include "tp_frame.h"
#include "sleep_cnt.h"
#include "t_retry.h"
#include "format_targ_key.h"
#include "send_msg.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "cws_insert.h"
#include "wcs_recover.h"
#include "wcs_sleep.h"
#include "have_crit.h"
#include "gdsbgtr.h"		/* for the BG_TRACE_PRO macros */

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	short			crash_count, dollar_tlevel;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	tp_frame		*tp_pointer;
GBLREF	trans_num		start_tn;
GBLREF	unsigned char		cw_set_depth, cw_map_depth, t_fail_hist[CDB_MAX_TRIES];
GBLREF	boolean_t		mu_reorg_process;
GBLREF	unsigned int		t_tries;
GBLREF	uint4			t_err;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		is_dollar_incr;

void t_retry(enum cdb_sc failure)
{
	tp_frame	*tf;
	unsigned char	*end, buff[MAX_ZWR_KEY_SZ];
	short		tl;

	error_def(ERR_GBLOFLOW);
	error_def(ERR_GVIS);
	error_def(ERR_TPRETRY);
	error_def(ERR_GVPUTFAIL);
	error_def(ERR_GVINCRFAIL);
	UNIX_ONLY(error_def(ERR_GVFAILCORE);)

	t_fail_hist[t_tries] = (unsigned char)failure;
	if (mu_reorg_process)
		CWS_RESET;
	if (0 == dollar_tlevel)
	{
		SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(cs_addrs, failure);	/* set wc_blocked if cache related status */
		cs_addrs->hdr->n_retries[t_tries]++;
		if (cs_addrs->critical)
			crash_count = cs_addrs->critical->crashcnt;
		if (cdb_sc_jnlclose == failure || cdb_sc_jnlstatemod == failure || cdb_sc_backupstatemod == failure)
			t_tries--;
		if ((CDB_STAGNATE <= ++t_tries) || (cdb_sc_future_read == failure) || (cdb_sc_helpedout == failure))
		{
			grab_crit(gv_cur_region);
			if ((dba_mm == cs_addrs->hdr->acc_meth) &&		/* we have MM and.. */
				(cs_addrs->total_blks != cs_addrs->ti->total_blks))	/* and file has been extended */
					wcs_recover(gv_cur_region);
			if (CDB_STAGNATE > t_tries)
				rel_crit(gv_cur_region);
			else  if (CDB_STAGNATE < t_tries)
			{
				if ((cdb_sc_helpedout == failure) || (cdb_sc_future_read == failure))
					t_tries = CDB_STAGNATE;		/* don't let it creep up on special cases */
				else if (cdb_sc_unfreeze_getcrit == failure)
				{
					GRAB_UNFROZEN_CRIT(gv_cur_region, cs_addrs, cs_data);
					t_tries = CDB_STAGNATE;
				} else
				{
					assert(cs_addrs->now_crit);
					rel_crit(gv_cur_region);

					if (NULL == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
						end = &buff[MAX_ZWR_KEY_SZ - 1];

					if (cdb_sc_gbloflow == failure)
						rts_error(VARLSTCNT(6) ERR_GBLOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
					if (IS_DOLLAR_INCREMENT)
					{
						assert(ERR_GVPUTFAIL == t_err);
						t_err = ERR_GVINCRFAIL;	/* print more specific error message */
					}
					UNIX_ONLY(send_msg(VARLSTCNT(9) t_err, 2, t_tries, t_fail_hist,
							   ERR_GVIS, 2, end-buff, buff, ERR_GVFAILCORE));
					UNIX_ONLY(gtm_fork_n_core());
					VMS_ONLY(send_msg(VARLSTCNT(8) t_err, 2, t_tries, t_fail_hist,
							   ERR_GVIS, 2, end-buff, buff));
					rts_error(VARLSTCNT(8) t_err, 2, t_tries, t_fail_hist, ERR_GVIS, 2, end-buff, buff);
				}
			}
		} else  if (cdb_sc_readblocked == failure)
		{
			if (TRUE == cs_addrs->now_crit)
			{
				assert(FALSE);
				rel_crit(gv_cur_region);
			}
			wcs_sleep(TIME_TO_FLUSH);
		}
		if ((cdb_sc_blockflush == failure) && !CCP_SEGMENT_STATE(cs_addrs->nl, CCST_MASK_HAVE_DIRTY_BUFFERS))
		{
			assert(cs_addrs->hdr->clustered);
			CCP_FID_MSG(gv_cur_region, CCTR_FLUSHLK);
			ccp_userwait(gv_cur_region, CCST_MASK_HAVE_DIRTY_BUFFERS, 0, cs_addrs->nl->ccp_cycle);
		}
		cw_set_depth = 0;
		cw_map_depth = 0;
		start_tn = cs_addrs->ti->curr_tn;
		assert(NULL != gv_target);
		if (NULL != gv_target)
			gv_target->clue.end = 0;
	} else
	{	/* for TP, do the minimum; most of the logic is in tp_retry, because it is also invoked directly from t_commit */
		t_fail_hist[t_tries] = failure;
		assert(NULL == cs_addrs || NULL != cs_addrs->hdr);	/* both cs_addrs and cs_data should be NULL or non-NULL. */
		assert(NULL != cs_addrs || cdb_sc_needcrit == failure); /* cs_addrs can be NULL in case of retry in op_lock2 */
		if (NULL != cs_addrs)					/*  in which case failure code should be cdb_sc_needcrit. */
		{
			SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(cs_addrs, failure);
			TP_RETRY_ACCOUNTING(cs_addrs, cs_addrs->hdr, failure);
		}
		assert((NULL != gv_target)
				|| (cdb_sc_needcrit == failure) && (CDB_STAGNATE <= t_tries) && have_crit(CRIT_HAVE_ANY_REG));
		/* only known case of gv_target being NULL is if a t_retry is done from gvcst_init. the above assert checks this */
		if (NULL != gv_target)
		{
			if (cdb_sc_blkmod != failure)
				TP_TRACE_HIST(CR_BLKEMPTY, gv_target);
			gv_target->clue.end = 0;
		}
		INVOKE_RESTART;
	}
}
