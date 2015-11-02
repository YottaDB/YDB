/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "anticipatory_freeze.h"

#ifdef DEBUG
STATICDEF	uint4		next_heartbeat_counter = 1; /* the heartbeat_counter at which enospc manipulations need to happen */
GBLDEF		boolean_t	is_jnlpool_creator; /* The purpose of this check is to assign only one process responsible of
						     * setting fake ENOSPC flags. If we had multiple processes messing with FREEZE,
						     * the test would freeze more often and eventually do nothing but freeze.
						     */
#endif

GBLREF	volatile uint4		heartbeat_counter;
GBLREF	boolean_t		is_src_server;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	uint4			process_id;
GBLREF	jnlpool_addrs		jnlpool;


#ifdef DEBUG

#define ENOSPC_GOOD_TO_BAD	2			/* 16 seconds */
#define ENOSPC_BAD_TO_GOOD	(2 * (rand() % 120 + 1)) /* 64 minutes */

error_def(ERR_TEXT);

char  choose_random_reg_list(char *enospc_enable_list, int n_reg)
{
	char count;
	int target, i;

	assert(n_reg <= 50);
	count = rand() % n_reg + 1;
	for (i = count; 0 < i; i--)
	{
		target = rand() % n_reg;
		/* JNL or DAT or BOTH? */
		enospc_enable_list[target] = rand() % 3 + 1;
	}
	return count;
}

void set_enospc_if_needed()
{
	int i;
	char enospc_enable_list[50]; /* JNL or DAT or BOTH? 0:NONE 1:DAT 2:JNL 3: BOTH */
	gd_addr *addr_ptr;
	gd_region *r_local, *r_top;
	sgmnt_addrs *csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(gtm_test_fake_enospc) && is_jnlpool_creator && ANTICIPATORY_FREEZE_AVAILABLE
	    && (next_heartbeat_counter == heartbeat_counter))
	{
		srand(time(NULL));
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{	/* Randomly simulate ENOSPC or free up space NO more than 50 regions are allowed. That is do to avoid
			 * malloc/free
			 */
			memset(enospc_enable_list, 0, 50);
			assert(addr_ptr->n_regions <= 50);
			/* Make sure to turn off FAKE ENOSPC for all regions if it is already on, otherwise test takes too long
			 * time.
			 */
			if (!IS_REPL_INST_FROZEN)
			{
				/* We are in GOOD state, going to BAD state, means we will be frozen due to ENOSPC */
				choose_random_reg_list(enospc_enable_list, addr_ptr->n_regions);
				next_heartbeat_counter = heartbeat_counter + ENOSPC_GOOD_TO_BAD;
			}
			else
				/* We are in BAD state, going to GOOD state, means disk space will be available */
				next_heartbeat_counter = heartbeat_counter + ENOSPC_BAD_TO_GOOD;
			for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions, i = 0;
			     r_local < r_top; r_local++, i++)
			{
				/* This assert checks each enospc_enable_list element is a valid one. See the enospc_enable_list
				 * declaration above to see what values it can take and what they mean.
				 */
				assert((enospc_enable_list[i] < 4) && (enospc_enable_list[i] >= 0));
				if (!r_local->open || r_local->was_open)
					continue;
				if ((dba_bg != r_local->dyn.addr->acc_meth) && (dba_mm != r_local->dyn.addr->acc_meth))
					continue;
				csa = &FILE_INFO(r_local)->s_addrs;
				if (ANTICIPATORY_FREEZE_ENABLED(csa))
				{
					switch(enospc_enable_list[i])
					{
					case 0:
						send_msg(VARLSTCNT(8) ERR_TEXT, 2, DB_LEN_STR(r_local), ERR_TEXT, 2,
							 LEN_AND_LIT("Turning off fake ENOSPC for both database and journal file.")
							);
						csa->nl->fake_db_enospc = FALSE;
						csa->nl->fake_jnl_enospc = FALSE;
						break;
					case 1:
						send_msg(VARLSTCNT(8) ERR_TEXT, 2, DB_LEN_STR(r_local), ERR_TEXT, 2,
							 LEN_AND_LIT("Turning on fake ENOSPC only for database file."));
						csa->nl->fake_db_enospc = TRUE;
						csa->nl->fake_jnl_enospc = FALSE;
						break;
					case 2:
						send_msg(VARLSTCNT(8) ERR_TEXT, 2, DB_LEN_STR(r_local), ERR_TEXT, 2,
							 LEN_AND_LIT("Turning on fake ENOSPC only for journal file."));
						csa->nl->fake_db_enospc = FALSE;
						csa->nl->fake_jnl_enospc = TRUE;
						break;
					case 3:
						send_msg(VARLSTCNT(8) ERR_TEXT, 2, DB_LEN_STR(r_local), ERR_TEXT, 2,
							 LEN_AND_LIT("Turning on fake ENOSPC for both database and journal file."));
						csa->nl->fake_db_enospc = TRUE;
						csa->nl->fake_jnl_enospc = TRUE;
						break;
					default:
						assert(FALSE);
					}

				}
			}
		}
	}
}
#endif

void heartbeat_timer(void)
{
	gd_addr			*addr_ptr;
	sgmnt_addrs		*csa;
	jnl_private_control	*jpc;
	gd_region		*r_local, *r_top;
	int			rc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	/* It will take heartbeat_counter about 1014 years to overflow. */
	heartbeat_counter++;
#	ifdef DEBUG
	set_enospc_if_needed();
#	endif
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
