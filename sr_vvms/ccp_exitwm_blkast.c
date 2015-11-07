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


/* Blocking AST routine entered when there is a Write-mode request from another machine */

void ccp_exitwm_blkast(ccp_db_header	**pdb)
{
	ccp_db_header	*db;


	db = *pdb;
	db->blocking_ast_received = TRUE;

	if (!db->wmexit_requested  &&  CCP_SEGMENT_STATE(db->segment->nl, CCST_MASK_WRITE_MODE))
	{
		db->wmexit_requested = TRUE;
		if (!db->quantum_expired  &&  db->segment->nl->ccp_state == CCST_DRTGNT)
			ccp_tick_start(db);
		ccp_exitwm_attempt(db);
	}

	return;
}
