/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "sleep_cnt.h"
#include "t_retry.h"
#include "format_targ_key.h"
#include "cws_insert.h"
#include "wcs_recover.h"
#include "wcs_sleep.h"


GBLREF gd_region	*gv_cur_region;
GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF short 		crash_count, dollar_tlevel;
GBLREF tp_frame		*tp_pointer;
GBLREF trans_num	start_tn;
GBLREF unsigned char	cw_set_depth, t_fail_hist[CDB_MAX_TRIES];
GBLREF unsigned int	t_tries;
GBLREF gv_namehead	*tp_fail_hist[CDB_MAX_TRIES];
GBLREF block_id		t_fail_hist_blk[CDB_MAX_TRIES];
GBLREF uint4		t_err;
GBLREF boolean_t	mu_reorg_process;

DEBUG_ONLY(GBLREF uint4 cumul_index;
	   GBLREF uint4 cu_jnl_index;
	  )

#define LCL_BUF_SIZE 512

void t_retry(enum cdb_sc failure)
{
	tp_frame	*tf;
	unsigned char	*end, buff[LCL_BUF_SIZE];
	short		tl;
	error_def(ERR_GBLOFLOW);
	error_def(ERR_GVIS);
	error_def(ERR_TPRETRY);
	error_def(ERR_GVPUTFAIL);

	t_fail_hist[t_tries] = (unsigned char)failure;
	if (mu_reorg_process)
		cws_reset();
	if (0 == dollar_tlevel)
	{
		cs_addrs->hdr->n_retries[t_tries]++;
		if (cs_addrs->critical)
			crash_count = cs_addrs->critical->crashcnt;
		if (cdb_sc_jnlclose == failure)
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

					if (NULL == (end = format_targ_key(buff, LCL_BUF_SIZE, gv_currkey, TRUE)))
						end = &buff[LCL_BUF_SIZE - 1];

					if (cdb_sc_gbloflow == failure)
						rts_error(VARLSTCNT(6) ERR_GBLOFLOW, 0, ERR_GVIS, 2, end - buff, buff);

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
		start_tn = cs_addrs->ti->curr_tn;
		gv_target->clue.end = 0;
		DEBUG_ONLY(
			/* gvcst_put currently writes a SET journal-record for every update and extra_block_split phase. This
			 * means that it may write more than one journal-record although only one update went in. This is
			 * because of the way the jnl code is structured in gvcst_put rather than in gvcst_put_blk
			 * Until that structuring is moved into gvcst_put_blk, we should take care not to reset cumul_index
			 * for gvcst_put which is identified by (ERR_GVPUTFAIL == t_err) or by (ERR_GVKILLFAIL != t_err).
			 */
			if (ERR_GVPUTFAIL != t_err)
				cumul_index = cu_jnl_index = 0;
		)
	} else
	{
		/* for TP, do the minimum; most of the logic is in tp_retry, because it is also invoked directly from t_commit */
		t_fail_hist[t_tries] = failure;
		TP_RETRY_ACCOUNTING(cs_addrs->hdr, failure);
		DEBUG_ONLY(
			if (cdb_sc_blkmod != failure)
				TP_TRACE_HIST(CR_BLKEMPTY, gv_target);
		)
		gv_target->clue.end = 0;
		INVOKE_RESTART;
	}
}
