/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

GBLREF	gd_region	*gv_cur_region;
GBLREF	gd_region	*ftok_sem_reg;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgmnt_data	*cs_data;

void mu_gv_cur_reg_init(void)
{
	gd_addr		*gdhdr;
	gd_region	*basedb_reg, *statsdb_reg;
	gd_segment	*basedb_seg, *statsdb_seg;

	gdhdr = create_dummy_gbldir();
	basedb_reg = gdhdr->regions;
	basedb_seg = basedb_reg->dyn.addr;
	statsdb_reg = basedb_reg + 1;
	statsdb_seg = statsdb_reg->dyn.addr;
	FILE_CNTL_INIT(basedb_seg);
	FILE_CNTL_INIT(statsdb_seg);
	gv_cur_region = gdhdr->regions;
}

void mu_gv_cur_reg_free(void)
{
	gd_addr		*gdhdr;
	gd_region	*basedb_reg, *statsdb_reg;
	gd_segment	*basedb_seg, *statsdb_seg;

	basedb_reg = gv_cur_region;
	gdhdr = basedb_reg->owning_gd;
	assert(basedb_reg == gdhdr->regions);
	assert(gv_cur_region != ftok_sem_reg);	/* ftok_sem_release should have been done BEFORE mu_gv_cur_reg_free */
	if (gv_cur_region == ftok_sem_reg)	/* Handle case nevertheless in pro */
	{	/* Before resetting gv_cur_region to NULL, also reset ftok_sem_reg to NULL. Not doing so would
		 * otherwise cause SIG-11 in "ftok_sem_release" (usually invoked as part of exit handling).
		 */
		ftok_sem_reg = NULL;
	}
	gv_cur_region = NULL; /* Now that gv_cur_region is going to be freed, make it inaccessible before starting the free */
	cs_addrs = NULL;
	cs_data = NULL;
	basedb_seg = basedb_reg->dyn.addr;
	FILE_CNTL_FREE(basedb_seg);
	statsdb_reg = basedb_reg + 1;
	assert(gdhdr == statsdb_reg->owning_gd);
	statsdb_seg = statsdb_reg->dyn.addr;
	FILE_CNTL_FREE(statsdb_seg);
	assert(NULL != gdhdr->id);
	if (NULL != gdhdr->id)
	{
		free(gdhdr->id);
		gdhdr->id = NULL;
	}
	if (NULL != gdhdr->tab_ptr)
	{
		free_hashtab_mname(gdhdr->tab_ptr);
		free(gdhdr->tab_ptr);
		gdhdr->tab_ptr = NULL;
	}
	free(gdhdr);
}
