/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "ccp.h"
#include "locks.h"
#include <psldef.h>
#include <lckdef.h>
#include <ssdef.h>
#include <efndef.h>


GBLREF	bool	ccp_stop, checkdb_timer;

static	int4	delta_30_sec[2] = { -300000000, -1 };


/* Close the database;  the process no longer has an interest in the region */

void ccp_tr_close( ccp_action_record *rec)
{
	ccp_db_header	*db;
	uint4	status;


	db = ccp_get_reg(&rec->v.file_id);

	if (db != NULL  &&  db->segment->nl->ccp_state != CCST_OPNREQ)
	{
		status = ccp_enqw(EFN$C_ENF, LCK$K_EXMODE, &db->refcnt_iosb, LCK$M_CONVERT | LCK$M_NOQUEUE, NULL, 0,
				  NULL, 0, NULL, PSL$C_USER, 0);
		/***** Check error status here? *****/

		if (status == SS$_NORMAL  ||  ccp_stop)
			if (db->segment->nl->ccp_state == CCST_RDMODE)
				ccp_close1(db);
			else
			{
				db->close_region = TRUE;
				if (db->wmexit_requested)
					status = sys$dclast(ccp_exitwm_attempt, db, PSL$C_USER);
				else
					status = sys$dclast(ccp_exitwm_blkast, &db->exitwm_timer_id, PSL$C_USER);
				if (status != SS$_NORMAL)
					ccp_signal_cont(status);	/***** Is this reasonable? *****/
			}

		/* NOTE:  The following `else' clause is a temporary kludge.  There appears to be a scenario in which
			  the CCP loses track of a request to relinquish write mode, causing GT.M processes to hang.
			  We piggyback on the checkdb timer processing here to guarantee that the hang doesn't persist,
			  by simulating the receipt of the blocking AST that initiates write mode exit processing. */
		else
			if (db->quantum_expired  &&  CCP_SEGMENT_STATE(db->segment->nl, CCST_MASK_WRITE_MODE))
			{
				status = sys$dclast(ccp_exitwm_blkast, &db->exitwm_timer_id, PSL$C_USER);
				if (status != SS$_NORMAL)
					ccp_signal_cont(status);	/***** Is this reasonable? *****/
			}
	}

	if (rec->pid == 0  &&  !checkdb_timer)
	{
		/* Request was from ccp_tr_checkdb */
		status = sys$setimr(0, delta_30_sec, ccp_tr_checkdb, 0, 0);
		if (status == SS$_NORMAL)
			checkdb_timer = TRUE;
		else
			ccp_signal_cont(status);	/***** Is this reasonable? *****/
	}

	return;
}
