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
#include "ccp.h"

void ccp_gotdrt_tick( ccp_db_header *db)
{

	if (!db->tick_in_progress && !db->quantum_expired)
	{	ccp_tick_start(db);
	}
	return;
}
