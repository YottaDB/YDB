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

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mu_gv_cur_reg_init.h"

GBLREF gd_region	*gv_cur_region;

void mu_gv_cur_reg_init(void)
{
	MALLOC_INIT(gv_cur_region, sizeof(gd_region));
	MALLOC_INIT(gv_cur_region->dyn.addr, sizeof(gd_segment));
	gv_cur_region->dyn.addr->acc_meth = dba_bg;

	FILE_CNTL_INIT(gv_cur_region->dyn.addr);
	gv_cur_region->dyn.addr->file_cntl->file_type = dba_bg;
}

void mu_gv_cur_reg_free(void)
{
	free(gv_cur_region->dyn.addr->file_cntl->file_info);
	free(gv_cur_region->dyn.addr->file_cntl);
	free(gv_cur_region->dyn.addr);
	free(gv_cur_region);
	gv_cur_region = NULL; /* If you free it, you must not access it */
}
