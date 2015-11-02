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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "op.h"

#define JNL_FENCE_MAX_LEVELS	255

error_def(ERR_TPMIXUP);
error_def(ERR_TRANSNEST);

GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	uint4			dollar_tlevel;
GBLREF	seq_num			seq_num_zero;
GBLREF	trans_num		local_tn;	/* transaction number for THIS PROCESS */
GBLREF	jnl_gbls_t		jgbl;

void	op_ztstart(void)
{
	if (dollar_tlevel)
		rts_error(VARLSTCNT(4) ERR_TPMIXUP, 2, "A fenced logical", "an M");
	if (jnl_fence_ctl.level >= JNL_FENCE_MAX_LEVELS)
		rts_error(VARLSTCNT(1) ERR_TRANSNEST);
	if (jnl_fence_ctl.level == 0)
	{
		jnl_fence_ctl.token = 0;
		jnl_fence_ctl.fence_list = JNL_FENCE_LIST_END;
		++local_tn;		/* Begin new local transaction */
		/* In journal recovery forward phase, we set jgbl.tp_ztp_jnl_upd_num to whatever update_num the journal record
		 * has so it is ok for the global variable to be a non-zero value at the start of a ZTP transaction (possible if
		 * ZTP of one process is in progress when ZTP of another process starts in the journal file). But otherwise
		 * (in GT.M runtime) we expect it to be 0 at beginning of each TP or ZTP.
		 */
		assert((0 == jgbl.tp_ztp_jnl_upd_num) || jgbl.forw_phase_recovery);
	}
	++jnl_fence_ctl.level;
}
