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
#include "ccpact.h"

void ccp_staleness(ccp_db_header **p)
{
	ccp_db_header *db;
	ccp_action_record buff;

	db = *p;
	db->stale_in_progress = FALSE;
	if (!db->write_mode_requested)
	{	buff.action = CCTR_WRITEDB;
		buff.pid = 0;
		buff.v.exreq.fid = ((vms_gds_info *)(db->greg->dyn.addr->file_cntl->file_info))->file_id;
		buff.v.exreq.cycle = db->segment->nl->ccp_cycle;
		ccp_act_request(&buff);
	}
	return;
}
