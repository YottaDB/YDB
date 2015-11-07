/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
GBLREF		boolean_t	is_jnlpool_creator; /* The purpose of this check is to assign only one process responsible of
						     * setting fake ENOSPC flags. If we had multiple processes messing with FREEZE,
						     * the test would freeze more often and eventually do nothing but freeze.
						     */
STATICDEF	uint4		syslog_deferred = 0;
#endif

GBLREF	volatile uint4		heartbeat_counter;
GBLREF	boolean_t		is_src_server;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	uint4			process_id;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	volatile int4		gtmMallocDepth;

#ifdef DEBUG

#define ENOSPC_FROZEN_DURATION		2				/* 16 seconds */
#define ENOSPC_UNFROZEN_DURATION	(2 * (rand() % 120 + 1))	/* 16 seconds to 32 minutes */
#define MAX_REGIONS			50

#define	NONE			0	/* 1/2 probablility */
#define	DB_ON			1	/* 1/6 probablility */
#define	JNL_ON			2	/* 1/6 probablility */
#define	DB_AND_JNL_ON		3	/* 1/6 probablility */
#define MAX_ENOSPC_TARGET	4

error_def(ERR_TEXT);
error_def(ERR_FAKENOSPCLEARED);

void  choose_random_reg_list(char *enospc_enable_list, int n_reg)
{
	int	i;

	assert(MAX_REGIONS >= n_reg);
	for (i = 0; i < n_reg; i++)
	{
		if(rand() % 2) /* Should we trigger fake ENOSPC on this region */
			enospc_enable_list[i] = rand() % (MAX_ENOSPC_TARGET - 1) + 1; /* What kind? */
		else
			enospc_enable_list[i] = NONE;
	}
}

void set_enospc_if_needed()
{
	gd_addr		*addr_ptr;
	char		enospc_enable_list[MAX_REGIONS];
	boolean_t	ok_to_interrupt, is_time_to_act;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	if (TREF(gtm_test_fake_enospc) && is_jnlpool_creator && ANTICIPATORY_FREEZE_AVAILABLE)
	{
		ok_to_interrupt = (INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) && (0 == gtmMallocDepth);
		is_time_to_act = (next_heartbeat_counter == heartbeat_counter);
		if (syslog_deferred && ok_to_interrupt)
		{
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_FAKENOSPCLEARED, 1, (heartbeat_counter - syslog_deferred));
			syslog_deferred = 0;
		}
		if (!is_time_to_act || syslog_deferred || (!ok_to_interrupt && !IS_REPL_INST_FROZEN))
		{
			/* We have to skip this because we have just fallen into deferred zone or we are currently in it */
			if (is_time_to_act)
				next_heartbeat_counter++;  /* Try again in the next heartbeat */
			return;
		}
		assert(0 == syslog_deferred);
		srand(time(NULL));
		addr_ptr = get_next_gdr(NULL);
		if (NULL == addr_ptr) /* Ensure that there is a global directory to operate on. */
			return;
		assert(NULL == get_next_gdr(addr_ptr));
		/* Randomly simulate ENOSPC or free space. NO more than 50 regions are allowed to avoid unnecessary
		 * malloc/frees in debug-only code
		 */
		assert(MAX_REGIONS >= addr_ptr->n_regions);
		if (!IS_REPL_INST_FROZEN)
		{	/* We are in an UNFROZEN state, and about to be FROZEN due to ENOSPC */
			choose_random_reg_list(enospc_enable_list, addr_ptr->n_regions);
			next_heartbeat_counter = heartbeat_counter + ENOSPC_FROZEN_DURATION;
		} else
		{	/* We are in a FROZEN state, and about to be UNFROZEN due to free space */
			memset(enospc_enable_list, 0, MAX_REGIONS);
			next_heartbeat_counter = heartbeat_counter + ENOSPC_UNFROZEN_DURATION;
			if (!ok_to_interrupt)
				syslog_deferred = heartbeat_counter;
		}
		set_enospc_flags(addr_ptr, enospc_enable_list, ok_to_interrupt);
	}
}

void set_enospc_flags(gd_addr *addr_ptr, char enospc_enable_list[], boolean_t ok_to_interrupt)
{
	gd_region	*r_local, *r_top;
	int		i;
	sgmnt_addrs	*csa;
	const char	*syslog_msg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions, i = 0;
	     r_local < r_top; r_local++, i++)
	{
		if ((dba_bg != r_local->dyn.addr->acc_meth) && (dba_mm != r_local->dyn.addr->acc_meth))
			continue;
		csa = REG2CSA(r_local);
		if ((NULL != csa) && (NULL != csa->nl) && ANTICIPATORY_FREEZE_ENABLED(csa))
		{
			switch(enospc_enable_list[i])
			{
			case NONE:
				syslog_msg = "Turning off fake ENOSPC for both database and journal file.";
				csa->nl->fake_db_enospc = FALSE;
				csa->nl->fake_jnl_enospc = FALSE;
				break;
			case DB_ON:
				syslog_msg = "Turning on fake ENOSPC only for database file.";
				csa->nl->fake_db_enospc = TRUE;
				csa->nl->fake_jnl_enospc = FALSE;
				break;
			case JNL_ON:
				syslog_msg = "Turning on fake ENOSPC only for journal file.";
				csa->nl->fake_db_enospc = FALSE;
				csa->nl->fake_jnl_enospc = TRUE;
				break;
			case DB_AND_JNL_ON:
				syslog_msg = "Turning on fake ENOSPC for both database and journal file.";
				csa->nl->fake_db_enospc = TRUE;
				csa->nl->fake_jnl_enospc = TRUE;
				break;
			default:
				assert(FALSE);
			}
			if (ok_to_interrupt)
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_TEXT, 2, DB_LEN_STR(r_local), ERR_TEXT, 2,
					     LEN_AND_STR(syslog_msg));
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
	DEBUG_ONLY(set_enospc_if_needed());
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
				csa = REG2CSA(r_local);
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
