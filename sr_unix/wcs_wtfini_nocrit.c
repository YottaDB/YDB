/****************************************************************
 *								*
 * Copyright (c) 2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include <error.h>

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "wcs_sleep.h"
#include "filestruct.h"
#include "gtmio.h"
#include "wcs_wt.h"
#include "sleep_cnt.h"
#include "aio_shim.h"

GBLREF uint4 		process_id;

error_def(ERR_AIOBUFSTUCK);
error_def(ERR_DBFILERR);

/* This function goes through the list of crs that is passed in and waits until all
 * of them have their AIO status available before returning. If the cr is no longer
 * dirty or another process has issued a write, the function goes to the next cr.
 */

void wcs_wtfini_nocrit(gd_region *reg, wtstart_cr_list_t *cr_list_ptr)
{
	int 		lcnt, ret;
	cache_rec_ptr_t	cr, *crbeg, *crtop, *crptr;

	crbeg = &cr_list_ptr->listcrs[0];
	crtop = crbeg + cr_list_ptr->numcrs;
	for (crptr = crbeg; crptr < crtop; crptr++)
	{
		cr = *crptr;
		for (lcnt = 1; cr->dirty && (cr->epid == process_id); lcnt++)
		{	/* dirty, i/o has been issued by our process and there is no i/o status available yet */
			AIO_SHIM_ERROR(&cr->aiocb, ret);
			if (EINPROGRESS != ret)
				break;
			if (0 == lcnt % SLEEP_ONE_MIN)
			{	/* let them know we have been waiting a while */
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(11) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_AIOBUFSTUCK, 5, (lcnt / SLEEP_ONE_MIN), cr->epid, cr->blk, cr->blk, EINPROGRESS);
			}
			wcs_sleep(lcnt); /* so wait a while */
		}
	}
}

