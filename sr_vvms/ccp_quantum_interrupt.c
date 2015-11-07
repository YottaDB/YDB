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

void ccp_quantum_interrupt( ccp_db_header **p)
{

	ccp_db_header *db;

	db = *p;
	assert(lib$ast_in_prog());
	if (!db->quantum_expired)
	{
		assert(db->segment->nl->ccp_state != CCST_WMXREQ);
		if (db->tick_in_progress)
		{
			db->tick_in_progress = FALSE;
			sys$cantim(&db->tick_timer_id, 0);
		}
		db->quantum_expired = TRUE;
	}
	ccp_exitwm_attempt(db);
	return;
}
