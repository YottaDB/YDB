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

#include <fab.h>

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"

GBLREF	ccp_db_header	*ccp_reg_root;


/* Entered after unsuccessful sys$open in ccp_opendb (via ccp_opendb1e) */

void ccp_tr_opendb1e( ccp_action_record	*rec)
{
	ccp_db_header	*db, *db0, *db1;
	uint4	status, status1;


	db = ccp_get_reg_by_fab(rec->v.fab);
	if (db == NULL)
		return;

	status = rec->v.fab->fab$l_sts;
	status1 = rec->v.fab->fab$l_stv;

	for (db0 = ccp_reg_root, db1 = NULL;  db0 != db;  db1 = db0, db0 = db0->next)
		;
	if (db1 == NULL)
		ccp_reg_root = db0->next;
	else
		db1->next = db0->next;

	free(FILE_INFO(db->greg)->fab->fab$l_nam);
	free(FILE_INFO(db->greg)->fab);
	free(db->greg->dyn.addr);
	free(db->greg);
	free(db);

	ccp_quemin_adjust(CCP_CLOSE_REGION);
	ccp_signal_cont(status, 0, status1);	/***** Is this reasonable? *****/

	return;
}
