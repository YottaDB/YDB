/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021-2024 YottaDB LLC and/or its subsidiaries.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stat.h"
#include "gtm_fcntl.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "gtmio.h"	/* for CLOSEFILE used by the F_CLOSE macro in JNL_FD_CLOSE */
#include "repl_sp.h"	/* for F_CLOSE used by the JNL_FD_CLOSE macro */
#include "iosp.h"	/* for SS_NORMAL used by the JNL_FD_CLOSE macro */
#include "gt_timer.h"
#include "gtmimagename.h"
#include "dpgbldir.h"
#include "have_crit.h"
#include "anticipatory_freeze.h"
#include "jnl_file_close_timer.h"
#include "db_snapshot.h"

GBLREF	boolean_t	oldjnlclose_started;
GBLREF	uint4		process_id;
GBLREF	boolean_t	exit_handler_active;

void jnl_file_close_timer(void)
{
	boolean_t		do_timer_restart = FALSE;
	gd_addr			*addr_ptr;
	gd_region		*r_local, *r_top;
	int			rc;
	jnl_private_control	*jpc;
	sgmnt_addrs		*csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	/* Check every 1 minute if we need to close an older generation journal file open; also close any lingering snapshot.
	 * The only exceptions are
	 *	a) The source server can have older generations open and they should not be closed.
	 *	b) If we are in the process of switching to a new journal file while we get interrupted
	 *		by the heartbeat timer, we should not close the older generation journal file
	 *		as it will anyways be closed by the mainline code. But identifying that we are in
	 *		the midst of a journal file switch is tricky so we check if the process is in
	 *		crit for this region and if so we skip the close this time and wait for the next heartbeat.
	 */
	if ((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) && !exit_handler_active)
	{
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{
			for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions; r_local < r_top; r_local++)
			{
				if (!r_local->open || r_local->was_open)
					continue;
				if (!IS_REG_BG_OR_MM(r_local))
					continue;
				csa = REG2CSA(r_local);
				SS_RELEASE_IF_NEEDED(csa, (node_local_ptr_t)csa->nl);
				jpc = csa->jnl;
				if (csa->now_crit)
				{
					do_timer_restart |= csa->snapshot_in_prog || ((NULL != jpc) && (NOJNL != jpc->channel));
					continue;
				}
				if ((NULL != jpc) && (NOJNL != jpc->channel) && JNL_FILE_SWITCHED(jpc))
				{	/* The journal file we have as open is not the latest generation journal file. Close it */
					/* Assert that we never have an active write on a previous generation journal file. */
					assert(process_id != jpc->jnl_buff->io_in_prog_latch.u.parts.latch_pid);
					JNL_FD_CLOSE(jpc->channel, rc);	/* sets jpc->channel to NOJNL */
					jpc->pini_addr = 0;
				}
				do_timer_restart |= csa->snapshot_in_prog || ((NULL != jpc) && (NOJNL != jpc->channel));
			}
		}
	}
	/* Only restart the timer if there are still journal files open, or if we didn't check because it wasn't safe.
	 * Otherwise, it will be restarted on the next successful jnl_file_open(), or never, if we are exiting.
	 */
	if (!exit_handler_active && (do_timer_restart || (INTRPT_OK_TO_INTERRUPT != intrpt_ok_state)))
		start_timer((TID)jnl_file_close_timer, OLDERJNL_CHECK_INTERVAL, jnl_file_close_timer, 0, NULL);
	else
	{
		assert(!do_timer_restart || exit_handler_active);
		oldjnlclose_started = FALSE;
	}
}
