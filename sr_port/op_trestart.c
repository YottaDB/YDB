/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
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
#include "gdsblk.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "cdb_sc.h"
#include "jnl.h"
#include "tp.h"
#include "tp_restart.h"
#include "op.h"
#include "tp_frame.h"

GBLREF	unsigned char	t_fail_hist[CDB_MAX_TRIES];
GBLREF	unsigned int	t_tries;
GBLREF	trans_num	tstart_local_tn;	/* copy of global variable "local_tn" at op_tstart time */
GBLREF	uint4		dollar_tlevel;
GBLREF	unsigned char	*tpstackbase, *tstart_readdr;
error_def(ERR_TLVLZERO);
error_def(ERR_TPRETRY);
error_def(ERR_TRESTMAX);

/* Sets t_fail_hist[t_tries] to cdb_sc_optrestart to indicate an explicit TP restart was requested by the user. Before doing
 * so, it checks if we are in the final retry and issues another error if the explicit restart is requested more than once.
 */
void	op_trestart_set_cdb_code(void)
{
	static	trans_num	trestart_final_retry_local_tn;
	static	uint4		trestart_final_retry_cnt;

	/* Since this function can be called from the TRESTART and ZMESSAGE (with msgid=ERR_TPRETRY) commands, it is not
	 * necessary we are in TP at this point. If so, tp_restart will signal the appropriate error so skip the final
	 * retry TP check if we are not in TP. This is necessary because the use of tstart_local_tn assumes we are in TP.
	 */
	if (dollar_tlevel && (CDB_STAGNATE == t_tries))
	{	/* If we are in the final retry and holding crit (if this TP transaction has opened at least one database
		 * at this point), we want to limit the # of times the user can restart this transaction as a TP restart
		 * entails wasted work all while holding crit on the db and preventing others from accessing the same.
		 */
		if (trestart_final_retry_local_tn != tstart_local_tn)
		{
			trestart_final_retry_cnt = 0;	/* Restart counting */
			trestart_final_retry_local_tn = tstart_local_tn;
		}
		trestart_final_retry_cnt++;
		assert(MAX_TP_FINAL_RETRY_TRESTART_CNT >= trestart_final_retry_cnt);
		if (MAX_TP_FINAL_RETRY_TRESTART_CNT <= trestart_final_retry_cnt)
		{	/* Currently TRESTMAX message text is framed based on the assumption that we do
			 * not allow more than one TRESTART in the final retry. If this ever changes (by
			 * changing the macro MAX_TP_FINAL_RETRY_TRESTART_CNT), we need to change the
			 * message text. The assert below is a reminder for this case.
			 */
			assert(1 == (MAX_TP_FINAL_RETRY_TRESTART_CNT - 1));
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_TRESTMAX);
		}
	}
	t_fail_hist[t_tries] = cdb_sc_optrestart;
}

/* The rtn_name and lbl_name function parameters are supplied by the XPEL parameter */
void	op_trestart(mval *rtn_name, mval *lbl_name, int newlevel)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!dollar_tlevel)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TLVLZERO);
		return; /* for the compiler only -- never executed */
	}
	op_trestart_set_cdb_code();
	assert(1 == newlevel);
	assert(MAX_MIDENT_LEN >= rtn_name->str.len);
	memcpy((TREF(trestart_xpel_rtn)).str.addr,rtn_name->str.addr, rtn_name->str.len);
	(TREF(trestart_xpel_rtn)).str.len = rtn_name->str.len;
	assert(MAX_MIDENT_LEN >= lbl_name->str.len);
	memcpy((TREF(trestart_xpel_lab)).str.addr,lbl_name->str.addr, lbl_name->str.len);
	(TREF(trestart_xpel_lab)).str.len = lbl_name->str.len;
	/* If we have parameters from the XPEL parameter, there is a tp stack and we have the tstart restart address
	*  then set the restart_pc to the tstart restart address. In case we are involved in a zgoto or error handling
	*  that may remove our frame set the dollar_zinxpel_clear_mpc to the tstart restart address. If we see that
	*  address as we are unwinding we will clean up XPEL processing
	*/
	if (((rtn_name->str.len > 0) || (lbl_name->str.len > 0)) && ((NULL != tpstackbase) && (NULL != tstart_readdr)))
	{
		TREF(in_xpel) = TRUE;
		(TREF(dollar_zinxpel))++;
		TREF(dollar_zinxpel_roll) = TRUE;
		((tp_frame *) (tpstackbase - sizeof(tp_frame)))->restart_pc = tstart_readdr;
		tstart_readdr = NULL; /* Has been placed in restart_pc so no longer useful */
	}
	INVOKE_RESTART;
}
