/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "tp_restart.h"
#include "op.h"

GBLREF	unsigned char	t_fail_hist[CDB_MAX_TRIES];
GBLREF	unsigned int	t_tries;
GBLREF	trans_num	tstart_local_tn;	/* copy of global variable "local_tn" at op_tstart time */
GBLREF	uint4		dollar_tlevel;

/* Sets t_fail_hist[t_tries] to cdb_sc_optrestart to indicate an explicit TP restart was requested by the user. Before doing
 * so, it checks if we are in the final retry and issues another error if the explicit restart is requested more than once.
 */
void	op_trestart_set_cdb_code(void)
{
	static	trans_num	trestart_final_retry_local_tn;
	static	uint4		trestart_final_retry_cnt;

	error_def(ERR_TRESTMAX);

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
			rts_error(VARLSTCNT(1) ERR_TRESTMAX);
		}
	}
	t_fail_hist[t_tries] = cdb_sc_optrestart;
}

void	op_trestart(int newlevel)
{
	error_def(ERR_TPRETRY);

	op_trestart_set_cdb_code();
	assert(1 == newlevel);	/* newlevel probably needs to become GBLREF assigned here and reset to 1 in tp_restart */
	INVOKE_RESTART;
}
