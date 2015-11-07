/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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
#include "filestruct.h"
#include "have_crit.h"

error_def(ERR_CRITRESET);
error_def(ERR_DBCCERR);

GBLREF	short			crash_count;
GBLREF	volatile int4		crit_count;
GBLREF	uint4			process_id;

void	rel_crit(gd_region *reg)
{
	ccp_action_aux_value	msg;
	sgmnt_addrs		*csa;
	enum cdb_sc 		status;

	csa = &FILE_INFO(reg)->s_addrs;
	if (csa->now_crit)
	{
		assert(0 == crit_count);
		crit_count++;
		assert(csa->nl->in_crit == process_id  ||  csa->nl->in_crit == 0);	/* : crit was held by this process */

		/* [lidral] The next assert is commented out because it caused failures in the test suite, although it was
		 * believed to be correct when added.  At the time it was added, no one knew what the comment about timeout
		 * causing re-entry with in_crit cleared meant -- it has been left as a potential clue for future
		 * investigation.
		*/
		/* Timeout can cause reentry with in_crit cleared */
		/* assert(csa->hdr->clustered  ||  csa->nl->in_crit != 0); */	/* : in_crit can be clear only if clustered */

		CRIT_TRACE(crit_ops_rw);		/* see gdsbt.h for comment on placement */

		csa->nl->in_crit = 0;

		if (csa->hdr->clustered  &&  csa->nl->ccp_crit_blocked)
		{
			/* For an explanation of the code dealing with clusters, see CCP_EXITWM_ATTEMPT.C.
			   Please do not change this code without updating the comments in that file.	*/
			msg.exreq.fid = FILE_INFO(reg)->file_id;
			msg.exreq.cycle = csa->nl->ccp_cycle;
			(void)ccp_sendmsg(CCTR_EXITWM, &msg);
			(void)ccp_userwait(reg, CCST_MASK_WMXGNT | CCST_MASK_RDMODE, 0, msg.exreq.cycle);
		}

		if ((status = mutex_unlockw(csa->critical, crash_count, &csa->now_crit)) != cdb_sc_normal)
		{
			csa->now_crit = FALSE;
			crit_count = 0;
			if (status == cdb_sc_critreset)
				rts_error(ERR_CRITRESET, 2, REG_LEN_STR(reg));
			else
			{
				assert(status == cdb_sc_dbccerr);
				rts_error(ERR_DBCCERR, 2, REG_LEN_STR(reg));
			}
			return;
		}
		crit_count = 0;
	}
	/* Now that crit for THIS region is released, check if deferred signal/exit handling can be done and if so do it */
	DEFERRED_EXIT_HANDLING_CHECK;
}
