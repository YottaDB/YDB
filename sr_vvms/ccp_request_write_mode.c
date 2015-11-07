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
#include "ccp.h"
#include "locks.h"
#include <lckdef.h>
#include <psldef.h>

GBLREF bool ccp_stop;


void ccp_request_write_mode( ccp_db_header *db)
{
	uint4	status;


	if (ccp_stop)
		return;

	db->write_mode_requested = TRUE;

	if (db->stale_in_progress)
	{
		sys$cantim(&db->stale_timer_id, PSL$C_USER);
		db->stale_in_progress = FALSE;
	}

	/* Convert Write-mode lock from Concurrent Read to Protected Write, reading the lock value block */
	status = ccp_enq(0, LCK$K_PWMODE, &db->wm_iosb, LCK$M_CONVERT | LCK$M_VALBLK, NULL, 0,
			 ccp_reqwm_interrupt, &db->wmcrit_timer_id, ccp_exitwm_blkast, PSL$C_USER, 0);
	/***** Check error status here? *****/

	return;
}
