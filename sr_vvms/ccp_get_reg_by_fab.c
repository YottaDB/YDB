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

GBLREF ccp_db_header *ccp_reg_root;

ccp_db_header *ccp_get_reg_by_fab(fb)
struct FAB *fb;
{
	ccp_db_header *ptr;

	assert(!lib$ast_in_prog());
	for ( ptr = ccp_reg_root ; ptr && ((vms_gds_info *)(ptr->greg->dyn.addr->file_cntl->file_info))->fab != fb; ptr = ptr->next)
		;
	return ptr;
}
