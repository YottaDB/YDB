/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <signal.h>	/* for VSIG_ATOMIC_T type */

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "ccp.h"
#include "ccpact.h"
#include "error.h"
#include "filestruct.h"
#include "wcs_recover.h"

error_def(ERR_CRITRESET);
error_def(ERR_DBCCERR);

GBLREF short			crash_count;
GBLREF volatile int4		crit_count;
GBLREF int4			exi_condition;
GBLREF VSIG_ATOMIC_T		forced_exit;
GBLREF uint4			process_id;
GBLREF sgmnt_addrs		*vms_mutex_check_csa;

/* One try to grab crit; no waiting because of possible deadlock.  Used by TP */

boolean_t grab_crit_immediate(gd_region *reg)
{
	unsigned short		cycle_count, cycle;
	ccp_action_aux_value	msg;
	sgmnt_addrs		*csa;
	enum cdb_sc		status;
	node_local_ptr_t	cnl;

	csa = &FILE_INFO(reg)->s_addrs;
	vms_mutex_check_csa = csa;
	cnl = csa->nl;
	if (!csa->now_crit)
	{
		assert(0 == crit_count);
		crit_count++;
		if (csa->hdr->clustered)
		{
			/* For an explanation of the code dealing with clusters, see CCP_EXITWM_ATTEMPT.C.
			   Please do not change this code without updating the comments in that file. */
			cycle = cnl->ccp_cycle;
			if (!CCP_SEGMENT_STATE(cnl, CCST_MASK_WRITE_MODE))
				return FALSE;
		}
		if ((status = mutex_lockwim(csa->critical, crash_count, &csa->now_crit)) != cdb_sc_normal)
		{
			crit_count = 0;
			switch (status)
			{
			case cdb_sc_nolock:
				return FALSE;
			case cdb_sc_critreset:
				rts_error_csa(CSA_ARG(NULL) ERR_CRITRESET, 2, REG_LEN_STR(reg));
			case cdb_sc_dbccerr:
				rts_error_csa(CSA_ARG(NULL) ERR_DBCCERR, 2, REG_LEN_STR(reg));
			default:
				if (forced_exit)
					EXIT(exi_condition);
				GTMASSERT;
			}
			return FALSE;
		}
		assert(cnl->in_crit == 0);
		cnl->in_crit = process_id;
		if (csa->hdr->clustered)
		{
			cycle = cnl->ccp_cycle;
			if (cnl->ccp_crit_blocked)
			{
				msg.exreq.fid = FILE_INFO(reg)->file_id;
				msg.exreq.cycle = cycle;
				(void)ccp_sendmsg(CCTR_EXITWM, &msg);
				(void)ccp_userwait(reg, ~(CCST_MASK_WRITE_MODE), 0, cycle);
				if (cnl->ccp_crit_blocked && (cnl->ccp_cycle == cycle) ||
				    !CCP_SEGMENT_STATE(cnl, CCST_MASK_WRITE_MODE))
				{
					crit_count = 0;
					rel_crit(reg);
					return FALSE;
				}
			}
		}
		crit_count = 0;
	}
	/* We can be in an AST if we are called wcs_wipchk_ast(). In that case don't do wcs_recover since it can
	 * cause deadlocks. Let the next guy obtaining crit do it. Note also the order of the statements in the
	 * if. wc_blocked is very rarely TRUE and hence is placed ahead of the lib$ast_in_prog check.
	 */
	if (cnl->wc_blocked && !lib$ast_in_prog())
		wcs_recover(reg);
	return TRUE;
}
