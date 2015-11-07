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

void ccp_tr_flushlk( ccp_action_record *rec)
{
	ccp_db_header *db;

	db = ccp_get_reg(&rec->v.file_id);
	assert(db);
	{
		/* file is open */
		if (CCP_SEGMENT_STATE(db->segment->nl, CCST_MASK_HAVE_DIRTY_BUFFERS))
			crit_wake(&rec->pid);
		else
		{
			ccp_pndg_proc_add(&db->flu_wait, rec->pid);
			if (!db->write_mode_requested)
				ccp_request_write_mode(db);
		}
	}
}
