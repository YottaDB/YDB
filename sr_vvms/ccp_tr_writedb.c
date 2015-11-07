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
#include "crit_wake.h"

GBLREF	bool	ccp_stop;


/* Request for write mode */

void ccp_tr_writedb( ccp_action_record	*rec)
{
	ccp_db_header	*db;


	db = ccp_get_reg(&rec->v.file_id);

	if (db == NULL)
	{
		if (rec->pid != 0  &&  !ccp_stop)
			ccp_opendb(rec);
	}
	else
		/* File is open or in the process of being opened */
		if (rec->pid != 0  &&  !db->segment->nl->ccp_crit_blocked  &&
		    CCP_SEGMENT_STATE(db->segment->nl, CCST_MASK_WRITE_MODE))
			crit_wake(&rec->pid);
		else
		{
			if (rec->pid != 0)
				ccp_pndg_proc_add(&db->write_wait, rec->pid);
			if (!db->write_mode_requested  &&  db->segment->nl->ccp_state != CCST_OPNREQ)
				ccp_request_write_mode(db);
		}

	return;
}
