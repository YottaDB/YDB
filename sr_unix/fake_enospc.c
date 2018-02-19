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

#ifdef DEBUG		/* The #ifdef DEBUG game is to basically leave a return statement, so picky compilers are satisfied */
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
#endif
#include "fake_enospc.h"
#ifdef DEBUG
GBLREF		jnlpool_addrs_ptr_t	jnlpool;
GBLREF		volatile int4		gtmMallocDepth;

STATICDEF	uint4		syslog_deferred = 0;

error_def(ERR_TEXT);
error_def(ERR_FAKENOSPCLEARED);

#define ENOSPC_FROZEN_DURATION		(16 * MILLISECS_IN_SEC)
#define ENOSPC_UNFROZEN_DURATION	(16 * MILLISECS_IN_SEC * ((rand() % 120) + 1))	/* 16 seconds to 32 minutes */
#define ENOSPC_GDWAIT_INTERVAL		(2 * MILLISECS_IN_SEC)
#define ENOSPC_RETRY_INTERVAL		MILLISECS_IN_SEC
#define DEFERRED_SYSLOG_INTERVAL	MILLISECS_IN_SEC
#define MAX_REGIONS			50
#define	NONE				0				/* 1/2 probablility */
#define	DB_ON				1				/* 1/6 probablility */
#define	JNL_ON				2				/* 1/6 probablility */
#define	DB_AND_JNL_ON			3				/* 1/6 probablility */
#define MAX_ENOSPC_TARGET		4
#endif

/* This is a debug-only timer routine which is run within the source server that creates the journal pool.
 * It randomly sets flags indicating that writes to database files and/or journal files within the instance
 * should return an ENOSPC error, expecting that the custom error retry processing will handle it seamlessly
 * when it later clears the flags.
 * The routine logs its activities to the syslog. If the timer fires in a context in which it is not safe
 * to send a message to the syslog, it starts a handle_deferred_syslog timer (below) to attempt to send the
 * syslog message at a future time.
 */
void fake_enospc(void)
{
#	ifdef DEBUG
	boolean_t	ok_to_interrupt;
	uint4		deferred_count;
	char		enospc_enable_list[MAX_REGIONS];
	const char	*syslog_msg;
	gd_addr		*addr_ptr;
	gd_region	*r_local, *r_top;
	int		i;
	sgmnt_addrs	*csa;
	jnlpool_addrs_ptr_t	local_jnlpool;	/* needed by INST_FREEZE_ON_NOSPC_ENABLED */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ok_to_interrupt = (INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) && (0 == gtmMallocDepth);
	if (syslog_deferred && ok_to_interrupt)
	{
		cancel_timer((TID)&syslog_deferred);
		deferred_count = syslog_deferred;
		syslog_deferred = 0;
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_FAKENOSPCLEARED, 1, deferred_count);
	}
	/* If ok_to_interrupt is FALSE and intrpt_ok_state == INTRPT_IN_SHMDT, it is possible we have detached
	 * from the shared memory (i.e. in the middle of the shmdt()) when the timer interrupt occurs and so
	 * we cannot do a IS_REPL_INST_FROZEN check which looks at a field inside jnlpool_ctl. Account for that below.
	 */
	if (syslog_deferred
		|| (!ok_to_interrupt && ((INTRPT_IN_SHMDT == intrpt_ok_state) || !IS_REPL_INST_FROZEN)) || !CUSTOM_ERRORS_LOADED)
	{	/* We have to skip this because we have just fallen into deferred zone or we are currently in it */
		/* Try again in a second */
		start_timer((TID)fake_enospc, ENOSPC_RETRY_INTERVAL, fake_enospc, 0, NULL);
		return;
	}
	assert(0 == syslog_deferred);
	addr_ptr = get_next_gdr(NULL);
	if (NULL == addr_ptr) /* Ensure that there is a global directory to operate on. */
	{
		start_timer((TID)fake_enospc, ENOSPC_GDWAIT_INTERVAL, fake_enospc, 0, NULL);
		return;
	}
	assert(NULL == get_next_gdr(addr_ptr));
	/* Randomly simulate ENOSPC or free space. NO more than 50 regions are allowed to avoid unnecessary
	 * malloc/frees in debug-only code
	 */
	assert(MAX_REGIONS >= addr_ptr->n_regions);
	if (!IS_REPL_INST_FROZEN)
	{	/* We are in an UNFROZEN state, and about to be FROZEN due to ENOSPC */
		assert(MAX_REGIONS >= addr_ptr->n_regions);
		for (i = 0; i < addr_ptr->n_regions; i++)
		{
			if (rand() % 2) /* Should we trigger fake ENOSPC on this region */
				enospc_enable_list[i] = rand() % (MAX_ENOSPC_TARGET - 1) + 1; /* What kind? */
			else
				enospc_enable_list[i] = NONE;
		}
		start_timer((TID)fake_enospc, ENOSPC_FROZEN_DURATION, fake_enospc, 0, NULL);
	} else
	{	/* We are in a FROZEN state, and about to be UNFROZEN due to free space */
		memset(enospc_enable_list, 0, MAX_REGIONS);
		if (!ok_to_interrupt)
		{
			syslog_deferred = 1;
			start_timer((TID)&syslog_deferred, DEFERRED_SYSLOG_INTERVAL, handle_deferred_syslog, 0, NULL);
		}
		start_timer((TID)fake_enospc, ENOSPC_UNFROZEN_DURATION, fake_enospc, 0, NULL);
	}
	for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions, i = 0; r_local < r_top; r_local++, i++)
	{
		if (!IS_REG_BG_OR_MM(r_local))
			continue;
		csa = REG2CSA(r_local);
		if ((NULL != csa) && (NULL != csa->nl) && INST_FREEZE_ON_NOSPC_ENABLED(csa, local_jnlpool))
		{
			syslog_msg = NULL;
			switch(enospc_enable_list[i])
			{
			case NONE:
				if (csa->nl->fake_db_enospc || csa->nl->fake_jnl_enospc)
				{
					syslog_msg = "Turning off fake ENOSPC for both database and journal file.";
					csa->nl->fake_db_enospc = FALSE;
					csa->nl->fake_jnl_enospc = FALSE;
				}
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
				break;				/* NOTREACHED */
			}
			if (ok_to_interrupt && (NULL != syslog_msg))
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_TEXT, 2, DB_LEN_STR(r_local), ERR_TEXT, 2,
					LEN_AND_STR(syslog_msg));
		}
	}

#	endif
	return;
}

/* This is a timer routine used by fake_enospc, above. */
void handle_deferred_syslog(void)
{
#	ifdef DEBUG
	boolean_t	ok_to_interrupt;
	uint4		deferred_count;

	if (syslog_deferred)
	{
		ok_to_interrupt = (INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) && (0 == gtmMallocDepth);
		if (ok_to_interrupt)
		{
			deferred_count = syslog_deferred;
			syslog_deferred = 0;
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_FAKENOSPCLEARED, 1, deferred_count);
		}
		else
		{
			syslog_deferred++;
			start_timer((TID)&syslog_deferred, DEFERRED_SYSLOG_INTERVAL, handle_deferred_syslog, 0, NULL);
		}
	}
#	endif
	return;
}
