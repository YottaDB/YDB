/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

#include "heartbeat_timer.h"
#include "gt_timer.h"
#include "gtmimagename.h"
#include "dpgbldir.h"
#include "have_crit.h"

GBLREF	volatile uint4		heartbeat_counter;
GBLREF	boolean_t		is_src_server;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	uint4			process_id;

void heartbeat_timer(void)
{
	gd_addr			*addr_ptr;
	gd_region		*r_top, *r_save, *r_local;
	sgmnt_addrs		*csa;
	jnl_private_control	*jpc;
	int			rc;

	/* It will take heartbeat_counter about 1014 years to overflow. */
	heartbeat_counter++;
	/* Check every 1 minute if we have an older generation journal file open. If so, close it.
	 * The only exceptions are
	 *	a) The source server can have older generations open and they should not be closed.
	 *	b) If we are in the process of switching to a new journal file while we get interrupted
	 *		by the heartbeat timer, we should not close the older generation journal file
	 *		as it will anyways be closed by the mainline code. But identifying that we are in
	 *		the midst of a journal file switch is tricky so we check if the process is in
	 *		crit for this region and if so we skip the close this time and wait for the next heartbeat.
	 */
	if ((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) && !is_src_server
		&& (0 == heartbeat_counter % NUM_HEARTBEATS_FOR_OLDERJNL_CHECK))
	{
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{
			for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions; r_local < r_top; r_local++)
			{
				if (!r_local->open || r_local->was_open)
					continue;
				if ((dba_bg != r_local->dyn.addr->acc_meth) && (dba_mm != r_local->dyn.addr->acc_meth))
					continue;
				csa = &FILE_INFO(r_local)->s_addrs;
				if (csa->now_crit)
					continue;
				jpc = csa->jnl;
				if ((NULL != jpc) && (NOJNL != jpc->channel) && JNL_FILE_SWITCHED(jpc))
				{	/* The journal file we have as open is not the latest generation journal file. Close it */
					/* Assert that we never have an active write on a previous generation journal file. */
					assert(process_id != jpc->jnl_buff->io_in_prog_latch.u.parts.latch_pid);
					JNL_FD_CLOSE(jpc->channel, rc);	/* sets jpc->channel to NOJNL */
					jpc->pini_addr = 0;
				}
			}
		}
	}
	start_timer((TID)heartbeat_timer, HEARTBEAT_INTERVAL, heartbeat_timer, 0, NULL);
}
