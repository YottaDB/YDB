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

#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "cdb_sc.h"
#include "error.h"
#include "iosp.h"		/* for declaration of SS_NORMAL */
#include "jnl.h"
#include "mv_stent.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"		/* for ATOI */
#include "send_msg.h"
#include "op.h"
#include "io.h"
#include "mlk_rollback.h"
#include "targ_alloc.h"
#include "getzposition.h"
#include "wcs_recover.h"
#include "tp_unwind.h"
#include "wcs_backoff.h"
#include "wcs_mm_recover.h"
#include "trans_log_name.h"
#include "tp_restart.h"

error_def(ERR_TLVLZERO);
error_def(ERR_TPFAIL);
error_def(ERR_TPRESTART);
error_def(ERR_TRESTNOT);
error_def(ERR_TRESTLOC);

#define	MAX_TRESTARTS	16

#ifdef DEBUG
static	int	tprestart_syslog_limit = 0;			/* limit TPRESTARTs */
static	int	tprestart_syslog_delta = 100000; 		/* limit TPRESTARTs */
static	int	num_tprestart = 0;
GBLDEF	int4	tp_fail_histtn[CDB_MAX_TRIES], tp_fail_bttn[CDB_MAX_TRIES];
GBLDEF	int4	tp_fail_n, tp_fail_level;
GBLDEF	int4	n_pvtmods, n_blkmods;
#endif

GBLREF	short		dollar_tlevel, dollar_trestart;
GBLREF	int		dollar_truth;
GBLREF	mval		dollar_zgbldir;
GBLREF	gd_addr		*gd_header;
GBLREF	gv_key		*gv_currkey;
GBLREF	gv_namehead	*gv_target;
GBLREF	stack_frame	*frame_pointer;
GBLREF	tp_frame	*tp_pointer;
GBLREF	sgm_info	*sgm_info_ptr;
GBLREF	tp_region	*tp_reg_list;		/* Chained list of regions used in this transaction not cleared on tp_restart */
GBLREF	mv_stent	*mv_chain;
GBLREF	unsigned char	*msp, *stackbase, *stacktop, t_fail_hist[CDB_MAX_TRIES];
GBLREF	sgm_info	*first_sgm_info;
GBLREF	gv_namehead	*tp_fail_hist[CDB_MAX_TRIES];
GBLREF	block_id	t_fail_hist_blk[CDB_MAX_TRIES];
GBLREF	unsigned int	t_tries;
GBLREF	int		process_id;
GBLREF	gd_region	*gv_cur_region;
GBLREF	bool		run_time;
GBLREF	bool		caller_id_flag;
GBLREF	trans_num	local_tn;	/* transaction number for THIS PROCESS */
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
#ifdef VMS
GBLREF	struct chf$signal_array	*tp_restart_fail_sig;
GBLREF	boolean_t	tp_restart_fail_sig_used;
#endif

CONDITION_HANDLER(tp_restart_ch)
{
	START_CH;
	/* On Unix, there is only one set of the signal info and this error will handily replace it. For VMS,
	   far more subterfuge is required. We will save the signal information and paramters and overlay the
	   TPRETRY signal information with it so that the signal will be handled properly. */
#ifdef VMS
	assert(NULL != tp_restart_fail_sig);
	assert(FALSE == tp_restart_fail_sig_used);
	assert(TPRESTART_ARG_CNT >= sig->chf$is_sig_args);
	memcpy(tp_restart_fail_sig, sig, (sig->chf$is_sig_args + 1) * sizeof(int));
	tp_restart_fail_sig_used = TRUE;
#endif
	UNWIND(NULL, NULL);
}

/* Note that adding a new rts_error in tp_restart() might need a change to the INVOKE_RESTART macro in tp.h and
 * TPRESTART_ARG_CNT in errorsp.h (sl_vvms). See comment in tp.h for INVOKE_RESTART macro for the details.
 */

void	tp_restart(int newlevel)
{
	unsigned char		*cp;
	short			tl, top;
	unsigned int		hist_index;
	sgm_info		*si;
	tp_frame		*tf;
	mv_stent		*mvc;
	tp_region		*tr;
	static gv_namehead	*noplace;
	mval			bangHere, beganHere;
	mstr			log_nam, trans_log_nam;
	char			trans_buff[MAX_FN_LEN+1];
	static int4		first_time = TRUE;
	sgmnt_addrs		*csa;

	ESTABLISH(tp_restart_ch);
	assert(1 == newlevel);
	if (0 == dollar_tlevel)
	{
		rts_error(VARLSTCNT(1) ERR_TLVLZERO);
		return; /* for the compiler only -- never executed */
	}

	/* Increment restart counts for each region in this transaction */
	for (tr = tp_reg_list;  NULL != tr;  tr = tr->fPtr)
		FILE_INFO(tr->reg)->s_addrs.hdr->n_tp_retries[t_tries]++;

	if (cdb_sc_normal != t_fail_hist[t_tries])
	{
#if defined(DEBUG)
		if (NULL != tp_fail_hist[t_tries])
		{
			for (cp = tp_fail_hist[t_tries]->clue.base, top = 0; (0 != *cp) && (sizeof(mident) >= top); cp++, top++)
				;
		} else
		{
			if (NULL == noplace)
			{
				noplace = (gv_namehead *)targ_alloc(sizeof("*UNKNOWN"));
				noplace->clue.end = sizeof("*UNKNOWN");
				memcpy(noplace->clue.base, "*UNKNOWN", sizeof("*UNKNOWN"));
			}
			tp_fail_hist[t_tries] = noplace;
			top = noplace->clue.end;
		}
		if (TRUE == first_time)
		{
			log_nam.addr = "$TPRESTART_SYSLOG_LIMIT";
			log_nam.len = sizeof("$TPRESTART_SYSLOG_LIMIT") - 1;
			if (trans_log_name(&log_nam, &trans_log_nam, trans_buff) == SS_NORMAL)
				tprestart_syslog_limit = ATOI(trans_log_nam.addr);

			log_nam.addr = "$TPRESTART_SYSLOG_DELTA";
			log_nam.len = sizeof("$TPRESTART_SYSLOG_DELTA") - 1;
			if (trans_log_name(&log_nam, &trans_log_nam, trans_buff) == SS_NORMAL)
				tprestart_syslog_delta = ATOI(trans_log_nam.addr);
			if (0 == tprestart_syslog_delta)
				tprestart_syslog_delta = 1;
			first_time = FALSE;
		}
		if (num_tprestart++ < tprestart_syslog_limit || 0 == (num_tprestart % tprestart_syslog_delta))
		{
			caller_id_flag = FALSE;		/* don't want caller_id in the operator log */
			if (cdb_sc_blkmod != t_fail_hist[t_tries])
			{
				send_msg(VARLSTCNT(14) ERR_TPRESTART, 12, t_tries + 1, t_fail_hist, t_fail_hist_blk[t_tries],
					(int)top, tp_fail_hist[t_tries]->clue.base, 0, 0, 0, 0,
					sgm_info_ptr->num_of_blks, sgm_info_ptr->cw_set_depth, local_tn);
			}
			else
			{
				send_msg(VARLSTCNT(14) ERR_TPRESTART, 12, t_tries + 1, t_fail_hist, t_fail_hist_blk[t_tries],
					(int)top, tp_fail_hist[t_tries]->clue.base, n_pvtmods, n_blkmods, tp_fail_level,
					tp_fail_n, sgm_info_ptr->num_of_blks, sgm_info_ptr->cw_set_depth, local_tn);
			}
			caller_id_flag = TRUE;
			n_pvtmods = n_blkmods = 0;
		}
#endif
	/* the following code is parallel, but not identical, to code in t_retry, which should be maintained in parallel */
		switch (t_fail_hist[t_tries])
		{
		case cdb_sc_helpedout:
			csa = &FILE_INFO(sgm_info_ptr->gv_cur_region)->s_addrs;
			if ((dba_bg == csa->hdr->acc_meth) && !csa->now_crit)
			{	/* The following grab/rel crit logic is purely to ensure that wcs_recover gets called if
				 * needed. This is because we saw wc_blocked to be TRUE in tp_tend and decided to restart.
				 */
				grab_crit(sgm_info_ptr->gv_cur_region);
				rel_crit(sgm_info_ptr->gv_cur_region);
			} else
			{
				assert(dba_mm == csa->hdr->acc_meth);
				wcs_recover(sgm_info_ptr->gv_cur_region);
			}
			if (CDB_STAGNATE > t_tries)
				break;
					/* WARNING - fallthrough !!! */
		case cdb_sc_future_read:
			t_tries = CDB_STAGNATE;		/* go straight to crit, pay $200 and do not pass go */
					/* WARNING - fallthrough !!! */
		case cdb_sc_unfreeze_getcrit:
		case cdb_sc_needcrit:
			/* Here when a final (4th) attempt has failed with a need for crit in some routine. The assumption is
			   that the previous attempt failed somewhere before transaction end therefore tp_reg_list did
			   not have a complete list of regions necessary to complete the transaction and therefore not all
			   the regions have been locked down. The new region (by virtue of it having now been referenced)
			   has been added to tp_reg_list so all we need now is a retry.
			 */
			assert(CDB_STAGNATE == t_tries);
			for (tr = tp_reg_list;  NULL != tr;  tr = tr->fPtr)
				rel_crit(tr->reg);	/* to ensure deadlock safe order, release all regions before retry */
			wcs_backoff(dollar_trestart * TP_DEADLOCK_FACTOR); /* Sleep so needed locks have a chance to get released */
			break;
		case cdb_sc_jnlclose:
			t_tries--;
			/* fall through */
		default:
			if (CDB_STAGNATE < ++t_tries)
			{
				hist_index = t_tries;
				t_tries = 0;
				assert(FALSE);
				rts_error(VARLSTCNT(4) ERR_TPFAIL, 2, hist_index + 1, t_fail_hist);
				return; /* for the compiler only -- never executed */
			} else
			{	/* as of this writing, this operates only between the 2nd and 3rd tries;
				 * the 2nd is fast with the assumption of coincidental confict in an attempt
				 * to take advantage of the buffer state created by the 1st try
				 * the next to last try is not followed by a backoff as it may leave the buffers locked,
				 * to reduce live lock and deadlock issues
				 * with only 4 tries that leaves only the "middle" for backoff.
				 */
				if ((0 < dollar_trestart) && ((CDB_STAGNATE - 1) > dollar_trestart));
						/* +1 to adjust for "lagging" dollar_trestart, +1 for windage */
					wcs_backoff(dollar_trestart + 2);
			}
		}
		if ((CDB_STAGNATE <= t_tries))
		{
			(void)tp_tend(TRUE);	/* call tp_tend just to grab crits */
			/* pick up all MM extension information */
			for (si = first_sgm_info; si != NULL; si = si->next_sgm_info)
				if (dba_mm == si->gv_cur_region->dyn.addr->acc_meth)
				{
					TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
					if (cs_addrs->total_blks < cs_addrs->ti->total_blks)
						wcs_mm_recover(si->gv_cur_region);
				}
		}
	}
	tl = dollar_tlevel;
	tf = tp_pointer;
	while(tl > newlevel)
	{
		tf = tf->old_tp_frame;
		--tl;
	}
	/* Before we get too far unwound here, if this is a nonrestartable transaction,
	   let's record where we are for the message later on. */
	if (FALSE == tf->restartable && run_time)
		getzposition(&bangHere);

        /* Do a rollback type cleanup (invalidate gv_target clues of read as well as
         * updated blocks). This is typically needed for a restart.
         */
        tp_clean_up(TRUE);
        mlk_rollback(newlevel);
	tp_unwind(newlevel, TRUE);
	gd_header = tp_pointer->gd_header;
	gv_target = tp_pointer->orig_gv_target;
	if (gv_target != NULL)
		gv_cur_region = gv_target->gd_reg;
	else
		gv_cur_region = NULL;
	TP_CHANGE_REG(gv_cur_region);
	dollar_tlevel = newlevel;
	top = gv_currkey->top;
	memcpy(gv_currkey, tp_pointer->orig_key, sizeof(*tp_pointer->orig_key) + tp_pointer->orig_key->end);
	gv_currkey->top = top;
	tp_pointer->fp->mpc = tp_pointer->restart_pc;
	tp_pointer->fp->ctxt = tp_pointer->restart_ctxt;
	while (frame_pointer != tf->fp)
		op_unwind();
	assert((msp <= stackbase) && (msp > stacktop));
	if (FALSE == tf->restartable)
	{
		if (run_time)
		{
			getzposition(&beganHere);
			send_msg(VARLSTCNT(1) ERR_TRESTNOT);		/* Separate msgs so we get both */
			send_msg(VARLSTCNT(6) ERR_TRESTLOC, 4, beganHere.str.len, beganHere.str.addr,
										bangHere.str.len, bangHere.str.addr);
			rts_error(VARLSTCNT(8) ERR_TRESTNOT, 0,
				ERR_TRESTLOC, 4, beganHere.str.len, beganHere.str.addr, bangHere.str.len, bangHere.str.addr);
		} else
			rts_error(VARLSTCNT(1) ERR_TRESTNOT);
		return; /* for the compiler only -- never executed */
	}
	++dollar_trestart;
	assert(MAX_TRESTARTS > dollar_trestart);	/* a magic number just to ensure we dont do too many restarts */
	if (!dollar_trestart)		/* in case of a wrap */
		dollar_trestart--;

	dollar_truth = tp_pointer->dlr_t;
	dollar_zgbldir = tp_pointer->zgbldir;
	assert((mv_chain <= (mv_stent *)stackbase) && (mv_chain > (mv_stent *)stacktop));
	for (mvc = mv_chain;  MVST_TPHOLD != mvc->mv_st_type;)
	{
		unw_mv_ent(mvc);
		mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc);
	}
	assert((int)mvc < (int)frame_pointer);
	mv_chain = mvc;
	msp = (unsigned char *)mvc;
	REVERT;
}
