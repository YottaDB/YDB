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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "ccp.h"
#include <ssdef.h>


/* AST routine called from ccp_exitwm_blkast, ccp_gotdrt_tick, and ccp_tick_interrupt */

void ccp_tick_start( ccp_db_header *db)
{
	uint4	status;


	assert(lib$ast_in_prog());
	assert(!db->quantum_expired);
	assert(db->wmexit_requested);
	assert(db->segment->nl->ccp_state != CCST_WMXREQ);

	db->tick_in_progress = TRUE;
	db->tick_tn = db->segment->ti->curr_tn;

	status = sys$setimr(0, &db->glob_sec->ccp_tick_interval, ccp_tick_interrupt, &db->tick_timer_id, 0);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	return;
}
