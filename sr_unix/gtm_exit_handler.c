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

#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/shm.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "io.h"
#include "iosp.h"
#include "iotimer.h"
#include "error.h"
#include "gtm_stdio.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmsecshr.h"
#include "gt_timer.h"
#ifdef UNIX
#include "mutex.h"
#endif
#include "op.h"
#include "zcall_package.h"
#include "gtm_exit_handler.h"
#include "gv_rundown.h"
#include "mprof.h"
#include "print_exit_stats.h"

GBLREF	int4		exi_condition;
GBLREF	short		dollar_tlevel;
GBLREF	int		process_exiting;
GBLREF	boolean_t	need_core;			/* Core file should be created */
GBLREF	boolean_t	created_core;			/* core file was created */
GBLREF	boolean_t	core_in_progress;
GBLREF	boolean_t	dont_want_core;
GBLREF	int4		process_id;
GBLREF	boolean_t	exit_handler_active;
GBLREF	boolean_t	pool_init;
GBLREF	jnlpool_addrs	jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF 	boolean_t	is_tracing_on;

void gtm_exit_handler(void)
{
        struct sigaction	act;

	if (exit_handler_active)	/* Don't recurse if exit handler exited */
		return;
	if (is_tracing_on)
		turn_tracing_off(NULL);
	exit_handler_active = TRUE;
	process_exiting = TRUE;
	cancel_timer(0);		/* Cancel all timers - No unpleasant surprises */
	ESTABLISH(lastchance1);
	secshr_db_clnup(NORMAL_TERMINATION);
	if (pool_init)
	{
		rel_lock(jnlpool.jnlpool_dummy_reg);
		mutex_cleanup(jnlpool.jnlpool_dummy_reg);
		SHMDT(jnlpool.jnlpool_ctl);
		jnlpool.jnlpool_ctl = jnlpool_ctl = NULL;
		pool_init = FALSE;
	}
	if (dollar_tlevel)
		op_trollback(0);
	zcall_halt();
	op_lkinit();
	op_unlock();
	op_zdeallocate(NO_M_TIMEOUT);
	REVERT;

	ESTABLISH(lastchance2);
	gv_rundown();
	REVERT;

	ESTABLISH(lastchance3);
	io_rundown(NORMAL_RUNDOWN);
	REVERT;

	print_exit_stats();
	if (need_core && !created_core && !dont_want_core)	/* We needed to core */
	{
		core_in_progress = TRUE;
		DUMP_CORE;		/* This will not return */
	}
}
