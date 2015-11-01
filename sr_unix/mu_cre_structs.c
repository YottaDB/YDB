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

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mu_cre_structs.h"

void mu_cre_structs(gd_region *greg)
{	greg->dyn.addr->file_cntl = (file_control *)malloc(sizeof(*greg->dyn.addr->file_cntl));
	memset(greg->dyn.addr->file_cntl, 0, sizeof(*greg->dyn.addr->file_cntl));
	greg->dyn.addr->file_cntl->file_info = (void *)malloc(sizeof(unix_db_info));
	memset(greg->dyn.addr->file_cntl->file_info, 0, sizeof(unix_db_info));
	return;
}
