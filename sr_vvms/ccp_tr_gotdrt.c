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
#include <psldef.h>
#include <ssdef.h>


/* Dirty buffers acquired;  entered after successful conversion of Flush-lock to Exclusive mode */

void ccp_tr_gotdrt( ccp_action_record	*rec)
{
	ccp_db_header	*db;
	uint4	status;
	void		ccp_exitwm_attempt(), ccp_gotdrt_tick();


	db = rec->v.h;

	assert(!db->dirty_buffers_acquired);
	db->dirty_buffers_acquired = TRUE;

	assert(db->segment->nl->ccp_state == CCST_WRTGNT);
	db->segment->nl->ccp_state = CCST_DRTGNT;

	if (db->blocking_ast_received  &&  !db->tick_in_progress  &&  !db->quantum_expired)
	{
		status = sys$dclast(ccp_gotdrt_tick, db, PSL$C_USER);
		if (status != SS$_NORMAL)
			ccp_signal_cont(status);	/***** Is this reasonable? *****/
	}

	ccp_pndg_proc_wake(&db->flu_wait);
	status = sys$dclast(ccp_exitwm_attempt, db, PSL$C_USER);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	return;
}
