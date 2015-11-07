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

GBLREF short			crash_count;
GBLREF volatile int4		crit_count;
GBLREF int4			exi_condition;
GBLREF uint4			process_id;
GBLREF sgmnt_addrs		*vms_mutex_check_csa;

error_def(ERR_CRITRESET);
error_def(ERR_DBCCERR);

void	grab_crit(gd_region *reg)
{
	unsigned short		cycle_count, cycle;
	ccp_action_aux_value	msg;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	enum cdb_sc		status;

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;

	vms_mutex_check_csa = csa;
	assert(!lib$ast_in_prog());
	if (!csa->now_crit)
	{
		assert(0 == crit_count);
		crit_count++;
		if (csd->clustered)
		{
			/* For an explanation of the code dealing with clusters, see CCP_EXITWM_ATTEMPT.C.
			   Please do not change this code without updating the comments in that file. */
			cycle = cnl->ccp_cycle;
			while (!CCP_SEGMENT_STATE(cnl, CCST_MASK_WRITE_MODE))
			{
				(void)ccp_sendmsg(CCTR_WRITEDB, &FILE_INFO(reg)->file_id);
				(void)ccp_userwait(reg, CCST_MASK_WRITE_MODE, 0, cycle);
				cycle = cnl->ccp_cycle;
			}
		}

		if (cdb_sc_normal !=
			(status = MUTEX_LOCKW(csa->critical, crash_count, &csa->now_crit, &csd->mutex_spin_parms)))
		{
			crit_count = 0;
			switch (status)
			{
			case cdb_sc_critreset:
				rts_error_csa(CSA_ARG(NULL) ERR_CRITRESET, 2, REG_LEN_STR(reg));
			case cdb_sc_dbccerr:
				rts_error_csa(CSA_ARG(NULL) ERR_DBCCERR, 2, REG_LEN_STR(reg));
			default:
				GTMASSERT;
			}
			return;
		}

		assert(cnl->in_crit == 0);
		cnl->in_crit = process_id;

		CRIT_TRACE(crit_ops_gw);		/* see gdsbt.h for comment on placement */

		if (csd->clustered)
		{
			cycle = cnl->ccp_cycle;
			if (cnl->ccp_crit_blocked)
			{
				msg.exreq.fid = FILE_INFO(reg)->file_id;
				msg.exreq.cycle = cycle;
				(void)ccp_sendmsg(CCTR_EXITWM, &msg);
				(void)ccp_userwait(reg, ~(CCST_MASK_WRITE_MODE), 0, msg.exreq.cycle);
				while (cnl->ccp_crit_blocked  &&  cnl->ccp_cycle == msg.exreq.cycle  ||
				       !CCP_SEGMENT_STATE(cnl, CCST_MASK_WRITE_MODE))
				{
					cycle = cnl->ccp_cycle;
					(void)ccp_sendmsg(CCTR_WRITEDB, &FILE_INFO(reg)->file_id);
					(void)ccp_userwait(reg, CCST_MASK_WRITE_MODE, 0, cycle);
				}
			}
		}
		crit_count = 0;
	}
	if (cnl->wc_blocked)
		wcs_recover(reg);
}
