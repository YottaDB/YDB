/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

ccp_db_header *ccp_get_reg(name)
gds_file_id *name;
{
	ccp_db_header *ptr;

	assert(!lib$ast_in_prog());
	/* note: should use ordered list for more efficiency */
	for ( ptr = ccp_reg_root ; ptr ; ptr = ptr->next)
	{	if (!memcmp(name, &((vms_gds_info *)(ptr->greg->dyn.addr->file_cntl->file_info))->file_id, SIZEOF(gds_file_id)))
			break;
	}
	return ptr;
}
