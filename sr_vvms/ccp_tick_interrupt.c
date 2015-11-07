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

void ccp_tick_interrupt( ccp_db_header **p)
{
	ccp_db_header *db;

	assert(lib$ast_in_prog());
	db = *p;
	if (!db->quantum_expired)
	{
		assert(db->wmexit_requested);
		assert(db->segment == NULL || db->segment->nl->ccp_state != CCST_WMXREQ);
		assert(db->tick_in_progress == TRUE);
		/* db->glob_sec->wcs_active_lvl -= db->drop_lvl; */
		if ((db->segment != NULL) && (db->tick_tn != db->segment->ti->curr_tn))
			ccp_tick_start(db);
		else
		{
			/* cancel quantum timer */
			sys$cantim(&db->quantum_timer_id, 0);
			db->tick_in_progress = FALSE;
			db->quantum_expired = TRUE;
			ccp_exitwm_attempt(db);
		}
	}
	return;
}
