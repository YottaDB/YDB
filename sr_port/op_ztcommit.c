/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
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

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;
GBLREF  jnl_fence_control       jnl_fence_ctl;
GBLREF  short                   dollar_tlevel;
GBLREF	seq_num			seq_num_zero;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	gd_region		*gv_cur_region;

void    op_ztcommit(int4 n)
{
	boolean_t			replication, yes_jnl_no_repl;
	uint4				jnl_status;
        sgmnt_addrs             	*csa, *next_csa;
	gd_region			*save_gv_cur_region;
        static struct_jrec_ztcom	ztcom_record = {{JRT_ZTCOM, ZTCOM_RECLEN, 0, 0, 0},
						0, 0, 0, {ZTCOM_RECLEN, JNL_REC_SUFFIX_CODE}};
	error_def(ERR_TRANSMINUS);
	error_def(ERR_TRANSNOSTART);
	error_def(ERR_REPLOFFJNLON);

	assert(ZTCOM_RECLEN == ztcom_record.suffix.backptr);
        if (n < 0)
                rts_error(VARLSTCNT(1) ERR_TRANSMINUS);
        if (jnl_fence_ctl.level == 0  ||  n > jnl_fence_ctl.level)
                rts_error(VARLSTCNT(1) ERR_TRANSNOSTART);
        assert(jnl_fence_ctl.level > 0);
        assert(dollar_tlevel == 0);

        if (n == 0)
                jnl_fence_ctl.level = 0;
        else
                jnl_fence_ctl.level -= n;
        if (0 != jnl_fence_ctl.level)
		return;
	if (!jgbl.forw_phase_recovery)
	{
		JNL_SHORT_TIME(ztcom_record.prefix.time);
		QWASSIGN(ztcom_record.token, jnl_fence_ctl.token); /* token was computed in the first call to
								      jnl_write_ztp_logical after op_ztstart */
		ztcom_record.participants = 0;
		for (csa = jnl_fence_ctl.fence_list;  csa != (sgmnt_addrs *) - 1;  csa = csa->next_fenced)
			ztcom_record.participants++;
	} else
	{
		ztcom_record.prefix.time = jgbl.gbl_jrec_time;
		QWASSIGN(ztcom_record.token, jgbl.mur_jrec_token_seq.token);
		QWASSIGN(ztcom_record.jnl_seqno, jgbl.mur_jrec_seqno);
		ztcom_record.participants = jgbl.mur_jrec_participants;
	}
	replication = yes_jnl_no_repl = FALSE;
	for (csa = jnl_fence_ctl.fence_list;  csa != (sgmnt_addrs *) - 1;  csa = csa->next_fenced)
	{
		if (REPL_ENABLED(csa))
		{
			assert(JNL_ENABLED(csa));
			replication = TRUE;
		} else if (JNL_ENABLED(csa)) {
			yes_jnl_no_repl = TRUE;
			save_gv_cur_region = csa->region;
		}
	}
	if (replication) /* instance is replicated */
	{
		if (yes_jnl_no_repl) /* journal is ON for a region in the replicated instance */
			rts_error(VARLSTCNT(4) ERR_REPLOFFJNLON, 2, DB_LEN_STR(save_gv_cur_region));
		grab_lock(jnlpool.jnlpool_dummy_reg);
	}

	/* Note that only those regions that are actively journaling will appear in the following list: */
	save_gv_cur_region = gv_cur_region; /* we change gv_cur_region in the loop, so save for later restore */
	for (csa = jnl_fence_ctl.fence_list;  csa != (sgmnt_addrs *) - 1;  csa = csa->next_fenced)
	{
		gv_cur_region = csa->jnl->region;
		tp_change_reg();
		grab_crit(gv_cur_region);
		jnl_status = jnl_ensure_open();
		if (jnl_status)
			rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csa->hdr), DB_LEN_STR(gv_cur_region));
		if (0 == csa->jnl->pini_addr)
			jnl_put_jrt_pini(csa);
		if (!jgbl.forw_phase_recovery)
		{
			if (REPL_ENABLED(csa))
				QWASSIGN(ztcom_record.jnl_seqno, temp_jnlpool_ctl->jnl_seqno);
			else
				QWASSIGN(ztcom_record.jnl_seqno, seq_num_zero);
		}
		ztcom_record.prefix.pini_addr = csa->jnl->pini_addr;
		ztcom_record.prefix.tn = csa->ti->curr_tn;
		ztcom_record.prefix.checksum = INIT_CHECKSUM_SEED;
		jnl_write(csa->jnl, JRT_ZTCOM, (jnl_record *)&ztcom_record, NULL, NULL);
		rel_crit(gv_cur_region);
		if (!jgbl.forw_phase_recovery)
			wcs_timer_start(gv_cur_region, TRUE);
	}
	gv_cur_region = save_gv_cur_region; /* restore original */
	tp_change_reg(); /* bring cs_* in sync with gv_cur_region */
	if (replication)
		rel_lock(jnlpool.jnlpool_dummy_reg);
	for (csa = jnl_fence_ctl.fence_list;  csa != (sgmnt_addrs *) - 1;  csa = next_csa)
	{       /* do the waits in a separate loop to prevent spreading out the transaction */
		if (!jgbl.forw_phase_recovery)
			jnl_wait(csa->jnl->region);
		next_csa = csa->next_fenced;
		csa->next_fenced = NULL;
	}
}
