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
#include "filestruct.h"
#include "ccp.h"
#include "ccpact.h"
#include "ccp_cluster_lock_wake.h"

void ccp_cluster_lock_wake(gd_region *reg)
{
	ccp_sendmsg(CCTR_LKRQWAKE, &((vms_gds_info *)(reg->dyn.addr->file_cntl->file_info))->file_id);
	return;
}
