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
#include "mu_gv_cur_reg_init.h"

GBLREF gd_region	*gv_cur_region;

void mu_gv_cur_reg_init(void)
{
	gv_cur_region = (gd_region *)malloc(sizeof(gd_region));
	memset(gv_cur_region, 0, sizeof(gd_region));

	gv_cur_region->dyn.addr = (gd_segment *)malloc(sizeof(gd_segment));
	memset(gv_cur_region->dyn.addr, 0, sizeof(gd_segment));
	gv_cur_region->dyn.addr->acc_meth = dba_bg;

	gv_cur_region->dyn.addr->file_cntl = (file_control *)malloc(sizeof(*gv_cur_region->dyn.addr->file_cntl));
	memset(gv_cur_region->dyn.addr->file_cntl, 0, sizeof(*gv_cur_region->dyn.addr->file_cntl));
	gv_cur_region->dyn.addr->file_cntl->file_type = dba_bg;

	gv_cur_region->dyn.addr->file_cntl->file_info = (GDS_INFO *)malloc(sizeof(GDS_INFO));
	memset(gv_cur_region->dyn.addr->file_cntl->file_info, 0, sizeof(GDS_INFO));
}

void mu_gv_cur_reg_free(void)
{
	free(gv_cur_region->dyn.addr->file_cntl->file_info);
	free(gv_cur_region->dyn.addr->file_cntl);
	free(gv_cur_region->dyn.addr);
	free(gv_cur_region);
	gv_cur_region = NULL; /* If you free it, you must not access it */
}
