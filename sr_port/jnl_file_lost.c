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

#include "gtm_inet.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "jnl.h"
#include "send_msg.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "anticipatory_freeze.h"

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	gd_region		*gv_cur_region;
GBLREF	volatile boolean_t	in_wcs_recover;
GBLREF	int			process_exiting;

error_def(ERR_JNLCLOSED);
error_def(ERR_REPLJNLCLOSED);

uint4 jnl_file_lost(jnl_private_control *jpc, uint4 jnl_stat)
{	/* Notify operator and terminate journaling */
	unsigned int	status;
	sgmnt_addrs	*csa;
	jnlpool_addrs_ptr_t	save_jnlpool;
	jnlpool_addrs_ptr_t	local_jnlpool;	/* needed by INST_FREEZE_ON_MSG_ENABLED */
	seq_num		reg_seqno, jnlseqno;
	boolean_t	was_lockid = FALSE, instfreeze_environ;

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	save_jnlpool = jnlpool;
	switch(jpc->region->dyn.addr->acc_meth)
	{
	case dba_mm:
	case dba_bg:
		csa = &FILE_INFO(jpc->region)->s_addrs;
		if (csa->jnlpool && (jnlpool != csa->jnlpool))
			jnlpool = csa->jnlpool;
		break;
	default:
		assertpro(FALSE && jpc->region->dyn.addr->acc_meth);
		/* no break */
	}
	assert(csa->now_crit);
	/* We issue an rts_error (instead of shutting off journaling) in the following cases :			{BYPASSOK}
	 * 1) $gtm_error_on_jnl_file_lost is set to issue runtime error (if not already issued) in case of journaling issues.
	 * 2) The process has the given message set in $gtm_custom_errors (indicative of instance freeze on error setup)
	 *    in which case the goal is to never shut-off journaling
	 */
	assert((NULL == jnlpool) || (NULL != jnlpool->jnlpool_ctl));
	instfreeze_environ = INST_FREEZE_ON_MSG_ENABLED(csa, jnl_stat, local_jnlpool);
	if ((JNL_FILE_LOST_ERRORS == TREF(error_on_jnl_file_lost)) || instfreeze_environ)
	{
		if (!process_exiting || instfreeze_environ || !csa->jnl->error_reported)
		{
			if (save_jnlpool != jnlpool)
				jnlpool = save_jnlpool;
			csa->jnl->error_reported = TRUE;
			in_wcs_recover = FALSE;	/* in case we're called in wcs_recover() */
			if (SS_NORMAL != jpc->status)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) jnl_stat, 4, JNL_LEN_STR(csa->hdr),
						DB_LEN_STR(gv_cur_region), jpc->status);
			else
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_stat, 4, JNL_LEN_STR(csa->hdr),
						DB_LEN_STR(gv_cur_region));
		}
		if (save_jnlpool != jnlpool)
			jnlpool = save_jnlpool;
		return jnl_stat;
	}
	if (0 != jnl_stat)
		jnl_send_oper(jpc, jnl_stat);
	csa->hdr->jnl_state = jnl_closed;
	jpc->jnl_buff->cycle++; /* increment shared cycle so all future callers of jnl_ensure_open recognize journal switch */
	assert(jpc->cycle < jpc->jnl_buff->cycle);
	if (REPL_ENABLED(csa->hdr))
	{
		csa->hdr->repl_state = repl_was_open;
		reg_seqno = csa->hdr->reg_seqno;
		jnlseqno = (jnlpool && jnlpool->jnlpool_ctl) ? jnlpool->jnlpool_ctl->jnl_seqno : MAX_SEQNO;
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_REPLJNLCLOSED, 6, DB_LEN_STR(jpc->region), &reg_seqno, &reg_seqno,
				&jnlseqno, &jnlseqno);
	} else
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_JNLCLOSED, 3, DB_LEN_STR(jpc->region), &csa->ti->curr_tn);
	jnl_file_close(jpc->region, FALSE, TRUE);
	if (save_jnlpool != jnlpool)
		jnlpool = save_jnlpool;
	return EXIT_NRM;
}
