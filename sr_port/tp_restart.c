/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#ifdef VMS
#include <descrip.h>
#endif

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
#include "rtnhdr.h"
#include "mv_stent.h"
#include "stack_frame.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"		/* for ATOI */
#include "send_msg.h"
#include "op.h"
#include "io.h"
#include "targ_alloc.h"
#include "getzposition.h"
#include "wcs_recover.h"
#include "tp_unwind.h"
#include "wcs_backoff.h"
#include "wcs_mm_recover.h"
#include "tp_restart.h"
#include "repl_msg.h"		/* for gtmsource.h */
#include "gtmsource.h"		/* for jnlpool_addrs structure definition */

error_def(ERR_TLVLZERO);
error_def(ERR_TPFAIL);
error_def(ERR_TPRESTART);
error_def(ERR_TRESTNOT);
error_def(ERR_TRESTLOC);
UNIX_ONLY(error_def(ERR_GVFAILCORE);)

#define	MAX_TRESTARTS		16
#define	FAIL_HIST_ARRAY_SIZE	32
#define	GVNAME_UNKNOWN		"*UNKNOWN"

static	int		num_tprestart = 0;
static	char		gvname_unknown[] = GVNAME_UNKNOWN;
static	int4		gvname_unknown_len = STR_LIT_LEN(GVNAME_UNKNOWN);

GBLDEF	int4		tprestart_syslog_limit;			/* limit TPRESTARTs */
GBLDEF	int4		tprestart_syslog_delta; 		/* limit TPRESTARTs */
GBLDEF	trans_num	tp_fail_histtn[CDB_MAX_TRIES], tp_fail_bttn[CDB_MAX_TRIES];
GBLDEF	int4		tp_fail_n, tp_fail_level;
GBLDEF	int4		n_pvtmods, n_blkmods;
GBLDEF	gv_namehead	*tp_fail_hist[CDB_MAX_TRIES];
GBLDEF	block_id	t_fail_hist_blk[CDB_MAX_TRIES];
GBLDEF	gd_region	*tp_fail_hist_reg[CDB_MAX_TRIES];

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
GBLREF	unsigned int	t_tries;
GBLREF	int		process_id;
GBLREF	gd_region	*gv_cur_region;
GBLREF	jnlpool_addrs	jnlpool;
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
	assert(TPRESTART_ARG_CNT >= sig->chf$is_sig_args);
	if (NULL == tp_restart_fail_sig)
		tp_restart_fail_sig = (struct chf$signal_array *)malloc((TPRESTART_ARG_CNT + 1) * sizeof(int));
	memcpy(tp_restart_fail_sig, sig, (sig->chf$is_sig_args + 1) * sizeof(int));
	assert(FALSE == tp_restart_fail_sig_used);
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
	mval			bangHere, beganHere;
	sgmnt_addrs		*csa;
	int4			num_closed = 0;
	boolean_t		tp_tend_status;
	mstr			gvname_mstr, reg_mstr;
	gd_region		*restart_reg;
	DEBUG_ONLY(
	static int4		uncounted_restarts;	/* do not count some failure codes towards MAX_TRESTARTS */
	static int4		t_fail_hist_index;
	static int4		t_fail_hist_array[FAIL_HIST_ARRAY_SIZE];
	)

	ESTABLISH(tp_restart_ch);
	assert(1 == newlevel);
	if (0 == dollar_tlevel)
	{
		rts_error(VARLSTCNT(1) ERR_TLVLZERO);
		return; /* for the compiler only -- never executed */
	}

	DEBUG_ONLY(
		if (uncounted_restarts >= dollar_trestart)
			uncounted_restarts = dollar_trestart;
	)
	/* Increment restart counts for each region in this transaction */
	for (tr = tp_reg_list;  NULL != tr;  tr = tr->fPtr)
	{
		if (tr->reg->open)
			FILE_INFO(tr->reg)->s_addrs.hdr->n_tp_retries[t_tries]++;
		else
		{
			assert(cdb_sc_needcrit == t_fail_hist[t_tries]);
			assert(!num_closed);	/* we can have at the most 1 region not opened in the whole tp_reg_list */
			num_closed++;
		}
	}

	if (tprestart_syslog_delta && (num_tprestart++ < tprestart_syslog_limit
				|| 0 == ((num_tprestart - tprestart_syslog_limit) % tprestart_syslog_delta)))
	{
		if (NULL != tp_fail_hist[t_tries])
			gvname_mstr = tp_fail_hist[t_tries]->gvname.var_name;
		else
		{
			gvname_mstr.addr = (char *)&gvname_unknown[0];
			gvname_mstr.len = gvname_unknown_len;
		}
		caller_id_flag = FALSE;		/* don't want caller_id in the operator log */
		assert(0 == cdb_sc_normal);
		if (cdb_sc_normal == t_fail_hist[t_tries])
			t_fail_hist[t_tries] = '0';	/* temporarily reset just for pretty printing */
		restart_reg = tp_fail_hist_reg[t_tries];
		if (NULL != restart_reg)
		{
			reg_mstr.len = restart_reg->dyn.addr->fname_len;
			reg_mstr.addr = (char *)&restart_reg->dyn.addr->fname[0];
		} else
		{
			reg_mstr.len = 0;
			reg_mstr.addr = NULL;
		}
		if (cdb_sc_blkmod != t_fail_hist[t_tries])
		{
			send_msg(VARLSTCNT(16) ERR_TPRESTART, 14, reg_mstr.len, reg_mstr.addr,
				t_tries + 1, t_fail_hist, t_fail_hist_blk[t_tries], gvname_mstr.len, gvname_mstr.addr,
				0, 0, 0, tp_blkmod_nomod,
				(NULL != sgm_info_ptr) ? sgm_info_ptr->num_of_blks : 0,
				(NULL != sgm_info_ptr) ? sgm_info_ptr->cw_set_depth : 0, &local_tn);
		} else
		{
			send_msg(VARLSTCNT(16) ERR_TPRESTART, 14, reg_mstr.len, reg_mstr.addr,
				t_tries + 1, t_fail_hist, t_fail_hist_blk[t_tries], gvname_mstr.len, gvname_mstr.addr,
				n_pvtmods, n_blkmods, tp_fail_level, tp_fail_n,
				sgm_info_ptr->num_of_blks,
				sgm_info_ptr->cw_set_depth, &local_tn);
		}
		tp_fail_hist_reg[t_tries] = NULL;
		tp_fail_hist[t_tries] = NULL;
		if ('0' == t_fail_hist[t_tries])
			t_fail_hist[t_tries] = cdb_sc_normal;	/* get back to where it was */
		caller_id_flag = TRUE;
		n_pvtmods = n_blkmods = 0;
	}
	/* We should never come here with a normal restart code unless it is the TRESTART command which resets t_tries to 0 */
	assert((cdb_sc_normal != t_fail_hist[t_tries]) || (0 == t_tries));
	DEBUG_ONLY(
		t_fail_hist_array[t_fail_hist_index++] = t_fail_hist[t_tries];
		if (FAIL_HIST_ARRAY_SIZE <= t_fail_hist_index)
			t_fail_hist_index = 0;
	)
	if (cdb_sc_normal != t_fail_hist[t_tries])
	{	/* the following code is parallel, but not identical, to code in t_retry, which should be maintained in parallel */
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
			DEBUG_ONLY(uncounted_restarts++;)
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
			{	/* regions might not have been opened if we t_retried in gvcst_init(). dont rel_crit in that case */
				if (tr->reg->open)
					rel_crit(tr->reg);  /* to ensure deadlock safe order, release all regions before retry */
			}
			if ((NULL != jnlpool.jnlpool_dummy_reg) && jnlpool.jnlpool_dummy_reg->open)
			{
				csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
				if (csa->now_crit)
					rel_lock(jnlpool.jnlpool_dummy_reg);
			}
			wcs_backoff(dollar_trestart * TP_DEADLOCK_FACTOR); /* Sleep so needed locks have a chance to get released */
			break;
		case cdb_sc_jnlclose:
		case cdb_sc_jnlstatemod:
		case cdb_sc_backupstatemod:
			t_tries--;
			DEBUG_ONLY(uncounted_restarts++;)
			/* fall through */
		default:
			if (CDB_STAGNATE < ++t_tries)
			{
				hist_index = t_tries;
				t_tries = 0;
				assert(FALSE);
				UNIX_ONLY(send_msg(VARLSTCNT(5) ERR_TPFAIL, 2, hist_index, t_fail_hist, ERR_GVFAILCORE));
				UNIX_ONLY(gtm_fork_n_core());
				VMS_ONLY(send_msg(VARLSTCNT(4) ERR_TPFAIL, 2, hist_index, t_fail_hist));
				rts_error(VARLSTCNT(4) ERR_TPFAIL, 2, hist_index, t_fail_hist);
				return; /* for the compiler only -- never executed */
			} else
			{	/* as of this writing, this operates only between the 2nd and 3rd tries;
				 * the 2nd is fast with the assumption of coincidental conflict in an attempt
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
		{	/* If there are any regions that haven't yet been opened, open them before attempting for crit on all.
			 * This is safe to do now since we don't hold any crit locks now so we can't create a deadlock.
			 */
			if (num_closed)
			{
				for (tr = tp_reg_list;  NULL != tr;  tr = tr->fPtr)
				{	/* to open region use gv_init_reg() instead of gvcst_init() since that does extra
					 * manipulations with gv_keysize, gv_currkey and gv_altkey.
					 */
					if (!tr->reg->open)
					{
						gv_init_reg(tr->reg);
						assert(tr->reg->open);
					}
				}
			}
			tp_tend_status = tp_crit_all_regions();	/* grab crits on all regions */
			assert(FALSE != tp_tend_status);
			/* pick up all MM extension information */
			for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)
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
	while (tl > newlevel)
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
	tp_unwind(newlevel, RESTART_INVOCATION);
	gd_header = tp_pointer->gd_header;
	gv_target = tp_pointer->orig_gv_target;
	if (NULL != gv_target)
		gv_cur_region = gv_target->gd_reg;
	else
		gv_cur_region = NULL;
	TP_CHANGE_REG(gv_cur_region);
	dollar_tlevel = newlevel;
	top = gv_currkey->top;
	/* ensure proper alignment before dereferencing tp_pointer->orig_key->end */
	assert(0 == (((unsigned long)tp_pointer->orig_key) % sizeof(tp_pointer->orig_key->end)));
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
	assert(dollar_trestart >= uncounted_restarts);
	assert(MAX_TRESTARTS > (dollar_trestart - uncounted_restarts)); /* a magic number to ensure we dont do too many restarts */
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
	assert((INTPTR_T)mvc < (INTPTR_T)frame_pointer);
	mv_chain = mvc;
	msp = (unsigned char *)mvc;
	REVERT;
}
