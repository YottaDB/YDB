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

GBLREF	bool		ccp_stop;
GBLREF	int		ccp_stop_ctr;
GBLREF 	ccp_db_header	*ccp_reg_root;

void ccp_tr_stop( ccp_action_record *r)
{
	ccp_db_header *ptr;
	ccp_action_record buff;

	if (ccp_stop)
		return;
	ccp_stop = TRUE;
	for (ptr = ccp_reg_root; ptr; ptr = ptr->next)
	{	ccp_stop_ctr++;
		buff.action = CCTR_CLOSE;
		buff.pid = 0;
		buff.v.file_id = ((vms_gds_info *)(ptr->greg->dyn.addr->file_cntl->file_info))->file_id;
		ccp_act_request(&buff);
	}
	return;
}
