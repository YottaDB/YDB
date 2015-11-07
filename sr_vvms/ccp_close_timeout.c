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
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "jnl.h"


/* AST routine entered on expiration of timer set in ccp_close1 */

void ccp_close_timeout(pdb)
ccp_db_header	**pdb;
{
	ccp_db_header	*db;


	assert(lib$ast_in_prog());

	db = *pdb;
	db->segment->jnl->jnl_buff->dskaddr = db->segment->jnl->jnl_buff->freeaddr;

	sys$wake(NULL, NULL);

	return;
}
