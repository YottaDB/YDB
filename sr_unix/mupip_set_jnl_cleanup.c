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
#include "gtm_unistd.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "error.h"
#include "jnl.h"
#include "mupipset.h"
#include "io.h"
#include "gtmsecshr.h"
#include "gds_rundown.h"
#include "ftok_sems.h"
#include "tp_change_reg.h"

GBLREF	mu_set_rlist	*grlist;
GBLREF	gd_region	*gv_cur_region;

void mupip_set_jnl_cleanup(void)
{
	mu_set_rlist	*rptr;

	for (rptr = grlist; rptr != NULL; rptr = rptr->fPtr)
	{
		if (rptr->state != ALLOCATED)
			continue;
		if (rptr->exclusive)
		{
			if (NULL != rptr->sd)
				free(rptr->sd);
			rptr->sd = NULL;
			if (-1 != rptr->fd)
				close(rptr->fd);
			db_ipcs_reset(rptr->reg, FALSE);
		}
		else
		{
			gv_cur_region = rptr->reg;
 			tp_change_reg();
			if (NULL != gv_cur_region->dyn.addr->file_cntl && NULL != rptr->sd)
				gds_rundown();
		}
		rptr->state = DEALLOCATED;
	}
}
