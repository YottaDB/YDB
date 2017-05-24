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
#include "mupip_set.h"
#include "io.h"
#include "gtmsecshr.h"
#include "gds_rundown.h"
#include "db_ipcs_reset.h"
#include "tp_change_reg.h"
#include "dbfilop.h"

GBLREF	mu_set_rlist	*grlist;
GBLREF	gd_region	*gv_cur_region;

error_def(ERR_NOTALLDBRNDWN);

void mupip_set_jnl_cleanup(void)
{
	mu_set_rlist		*rptr;
	file_control		*fc;
	int4			rundown_status = EXIT_NRM;		/* if "gds_rundown" went smoothly */

	for (rptr = grlist; NULL != rptr; rptr = rptr->fPtr)
	{
		if (ALLOCATED == rptr->state && !rptr->exclusive)
		{
			assert(NULL != rptr->reg->dyn.addr->file_cntl && NULL != rptr->sd);
			if (NULL != rptr->reg->dyn.addr->file_cntl && NULL != rptr->sd)
				rel_crit(rptr->reg);
		}
	}

	for (rptr = grlist; NULL != rptr; rptr = rptr->fPtr)
	{
		if (ALLOCATED != rptr->state)
			continue;
		gv_cur_region = rptr->reg;
		if (rptr->exclusive)
		{
			assert(NULL != rptr->sd);
			if (NULL == rptr->sd)
				continue;
			free(rptr->sd);
			rptr->sd = NULL;
			fc = gv_cur_region->dyn.addr->file_cntl;
			fc->op = FC_CLOSE;
			dbfilop(fc);
			db_ipcs_reset(gv_cur_region);
		} else
		{
 			tp_change_reg();
			assert(NULL != gv_cur_region->dyn.addr->file_cntl && NULL != rptr->sd);
			if (NULL != gv_cur_region->dyn.addr->file_cntl && NULL != rptr->sd)
				rundown_status |= gds_rundown(CLEANUP_UDI_TRUE);
			/* Note: We did not allocate, so we do not deallocate rptr->sd */
			rptr->sd = NULL;
		}
		rptr->state = DEALLOCATED;
	}

	if (EXIT_NRM != rundown_status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTALLDBRNDWN);
}
