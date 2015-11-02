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
#include "gtm_time.h"
#include "gtm_inet.h"
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "op.h"
#include "jnl_write.h"
#include "wcs_timer_start.h"
#include "tp_change_reg.h"
#include "jnl_get_checksum.h"
#include "iosp.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;
GBLREF  jnl_fence_control       jnl_fence_ctl;
GBLREF  uint4			dollar_tlevel;
GBLREF	seq_num			seq_num_zero;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;

error_def(ERR_REPLOFFJNLON);
error_def(ERR_TRANSMINUS);
error_def(ERR_TRANSNOSTART);

void    op_ztcommit(int4 n)
{
	boolean_t			replication, yes_jnl_no_repl;
	uint4				jnl_status;
        sgmnt_addrs			*csa, *csa_next, *new_fence_list, *tcsa, **tcsa_insert;
	gd_region			*save_gv_cur_region;
	jnl_private_control		*jpc;
	sgmnt_data_ptr_t		csd;
	jnl_buffer_ptr_t		jbp;
	jnl_tm_t			save_gbl_jrec_time;
	int4				prev_index;
        static struct_jrec_ztcom	ztcom_record = {{JRT_ZTCOM, ZTCOM_RECLEN, 0, 0, 0, 0},
						0, 0, 0, 0, {ZTCOM_RECLEN, JNL_REC_SUFFIX_CODE}};

	assert(ZTCOM_RECLEN == ztcom_record.suffix.backptr);
        if (n < 0)
                rts_error(VARLSTCNT(1) ERR_TRANSMINUS);
        if (jnl_fence_ctl.level == 0  ||  n > jnl_fence_ctl.level)
                rts_error(VARLSTCNT(1) ERR_TRANSNOSTART);
        assert(jnl_fence_ctl.level > 0);
        assert(!dollar_tlevel);

        if (n == 0)
                jnl_fence_ctl.level = 0;
        else
                jnl_fence_ctl.level -= n;
        if (0 != jnl_fence_ctl.level)
		return;
	if (!jgbl.forw_phase_recovery)
	{
		SET_GBL_JREC_TIME;
		ztcom_record.participants = 0;
		for (csa = jnl_fence_ctl.fence_list; JNL_FENCE_LIST_END != csa; csa = csa->next_fenced)
			ztcom_record.participants++;
	} else
		ztcom_record.participants = jgbl.mur_jrec_participants;
	/* If GT.M, token was computed in the first call to jnl_write_ztp_logical after op_ztstart.
	 * If journal recovery, token was set in mur_output_record.
	 */
	QWASSIGN(ztcom_record.token, jnl_fence_ctl.token);
	replication = yes_jnl_no_repl = FALSE;
	new_fence_list = JNL_FENCE_LIST_END;
	/* Sort journaled regions based on ftok order and grab crit. Do this BEFORE grabbing jnlpool lock to avoid deadlock */
	for (csa = jnl_fence_ctl.fence_list; JNL_FENCE_LIST_END != csa; csa = csa_next)
	{
		if (REPL_ALLOWED(csa))
		{
			assert(JNL_ENABLED(csa) || REPL_WAS_ENABLED(csa));
			replication = TRUE;
		} else if (JNL_ENABLED(csa))
		{
			yes_jnl_no_repl = TRUE;
			save_gv_cur_region = csa->region;
		}
		csa_next = csa->next_fenced;
		tcsa_insert = &new_fence_list;
		for (tcsa = *tcsa_insert; JNL_FENCE_LIST_END != tcsa; tcsa_insert = &tcsa->next_fenced, tcsa = *tcsa_insert)
		{
			assert(csa->fid_index != tcsa->fid_index);
			if (csa->fid_index < tcsa->fid_index)
				break;
		}
		csa->next_fenced = tcsa;
		*tcsa_insert = csa;
	}
	save_gv_cur_region = gv_cur_region; /* we change gv_cur_region in the loop below, so save for later restore */
	DEBUG_ONLY(prev_index = 0;)
	jnl_fence_ctl.fence_list = new_fence_list;
	/* Note that only those regions that are actively journaling will appear in the following list: */
	for (csa = new_fence_list; JNL_FENCE_LIST_END != csa; csa = csa->next_fenced)
	{
		assert(prev_index < csa->fid_index);
		DEBUG_ONLY(prev_index = csa->fid_index;)
		jpc = csa->jnl;
		gv_cur_region = jpc->region;	/* needed for jnl_ensure_open */
		tp_change_reg();		/* needed for jnl_ensure_open */
		assert(csa == cs_addrs);
		csd = csa->hdr;
		if (!csa->hold_onto_crit)
			grab_crit(gv_cur_region);
		assert(csa->now_crit);
		if (JNL_ENABLED(csd))
		{
			jbp = jpc->jnl_buff;
			/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order of jnl
			 * records. This needs to be done BEFORE the jnl_ensure_open as that could write journal records
			 * (if it decides to switch to a new journal file)
			 */
			ADJUST_GBL_JREC_TIME(jgbl, jbp);
			jnl_status = jnl_ensure_open();
			if (jnl_status)
			{
				if (SS_NORMAL != jpc->status)
					rts_error(VARLSTCNT(7) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region),
						jpc->status);
				else
					rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
			}
		}
	}
	gv_cur_region = save_gv_cur_region; /* restore original */
	tp_change_reg(); /* bring cs_* in sync with gv_cur_region */
	DEBUG_ONLY(save_gbl_jrec_time = jgbl.gbl_jrec_time;)
	if (replication) /* instance is replicated */
	{
		if (yes_jnl_no_repl) /* journal is ON but replication is OFF for a region in the replicated instance */
			rts_error(VARLSTCNT(4) ERR_REPLOFFJNLON, 2, DB_LEN_STR(save_gv_cur_region));
	}
	for (csa = new_fence_list; JNL_FENCE_LIST_END != csa; csa = csa->next_fenced)
	{
		jpc = csa->jnl;
		assert(csa->now_crit);
		if (0 == jpc->pini_addr)
			jnl_put_jrt_pini(csa);
		ztcom_record.prefix.pini_addr = jpc->pini_addr;
		ztcom_record.prefix.tn = csa->ti->curr_tn;
		ztcom_record.prefix.checksum = INIT_CHECKSUM_SEED;
		ztcom_record.prefix.time = jgbl.gbl_jrec_time;
		ztcom_record.prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED,
									(uint4 *)&ztcom_record, SIZEOF(struct_jrec_ztcom));
		JNL_WRITE_APPROPRIATE(csa, jpc, JRT_ZTCOM, (jnl_record *)&ztcom_record, NULL, NULL);
		if (!csa->hold_onto_crit)
			rel_crit(jpc->region);
	}
	/* Ensure jgbl.gbl_jrec_time did not get reset by any of the jnl writing functions. This is necessary to ensure that
	 * the same timestamp is written in the ZTCOM record for ALL regions (which is currently required by journal recovery).
	 */
	assert(save_gbl_jrec_time == jgbl.gbl_jrec_time);
	for (csa = new_fence_list; JNL_FENCE_LIST_END != csa; csa = csa_next)
	{       /* do the waits in a separate loop to prevent spreading out the transaction */
		if (!jgbl.forw_phase_recovery)
		{
			jpc = csa->jnl;
			wcs_timer_start(jpc->region, TRUE);
			jnl_wait(jpc->region);
		}
		csa_next = csa->next_fenced;
		csa->next_fenced = NULL;
	}
	jgbl.tp_ztp_jnl_upd_num = 0;
}
