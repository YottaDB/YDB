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
#include "filestruct.h"
#include "ccp.h"
#include <ssdef.h>

GBLREF	ccp_db_header	*ccp_reg_root;
GBLDEF	bool		checkdb_timer;

static	int4		delta_30_sec[2] = { -300000000, -1 };


/* Check to see if any databases are still being accessed */

void ccp_tr_checkdb(void)
{
	ccp_db_header		*db;
	ccp_action_record	request;
	uint4		status;


	checkdb_timer = FALSE;

	if (ccp_reg_root == NULL)
	{
		status = sys$setimr(0, delta_30_sec, ccp_tr_checkdb, 0, 0);
		if (status == SS$_NORMAL)
			checkdb_timer = TRUE;
		else
			ccp_signal_cont(status);	/***** Is this reasonable? *****/
	}
	else
		for (db = ccp_reg_root;  db != NULL;  db = db->next)
		{
			request.action = CCTR_CLOSE;
			request.pid = 0;
			request.v.file_id = FILE_INFO(db->greg)->file_id;
			if (!ccp_act_request(&request))
				break;
		}
		/* Let ccp_tr_close restart the timer so we don't build up redundant requests */

	return;
}
