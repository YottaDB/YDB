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

#include "gtm_unistd.h"
#include "gtm_inet.h"

#include <signal.h>
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
#include "gt_timer.h"
#ifdef UNIX
#include "mutex.h"
#endif
#include "op.h"
#include "fgncalsp.h"
#include "zcall_package.h"
#include "gtm_exit_handler.h"
#include "gv_rundown.h"
#include "mprof.h"
#include "print_exit_stats.h"
#include "invocation_mode.h"
#include "secshr_db_clnup.h"

GBLREF	int4			exi_condition;
GBLREF	uint4			dollar_tlevel;
GBLREF	boolean_t		need_core;			/* Core file should be created */
GBLREF	boolean_t		created_core;			/* core file was created */
GBLREF	unsigned int		core_in_progress;
GBLREF	boolean_t		dont_want_core;
GBLREF	int4			process_id;
GBLREF	boolean_t		exit_handler_active;
GBLREF	boolean_t		pool_init;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF 	boolean_t		is_tracing_on;

#ifdef DEBUG
GBLREF	int			process_exiting;
GBLREF	boolean_t		ok_to_UNWIND_in_exit_handling;
#endif

void gtm_exit_handler(void)
{
        struct sigaction	act;
	struct extcall_package_list *package_ptr;
        DCL_THREADGBL_ACCESS;

        SETUP_THREADGBL_ACCESS;
	if (exit_handler_active)	/* Don't recurse if exit handler exited */
		return;
	if (is_tracing_on)
		turn_tracing_off(NULL);
	exit_handler_active = TRUE;
	SET_PROCESS_EXITING_TRUE;
	cancel_timer(0);		/* Cancel all timers - No unpleasant surprises */
	ESTABLISH(lastchance1);
	secshr_db_clnup(NORMAL_TERMINATION);
	if (dollar_tlevel)
		OP_TROLLBACK(0);
	zcall_halt();
	op_lkinit();
	op_unlock();
	op_zdeallocate(NO_M_TIMEOUT);
	REVERT;

	ESTABLISH(lastchance2);
	gv_rundown();
	REVERT;

	ESTABLISH(lastchance3);
	/* Invoke cleanup routines for all the shared libraries loaded during external call initialisation.
 	 * The cleanup routines are not mandatory routines, but if defined, will be invoked before
	 * closing the shared library.
	 */
	for (package_ptr = TREF(extcall_package_root); package_ptr; package_ptr = package_ptr->next_package)
	{
	    if (package_ptr->package_clnup_rtn)
		package_ptr->package_clnup_rtn();
	    fgn_closepak(package_ptr->package_handle, INFO);
	}
	/* We know of at least one case where the below code would error out. That is if this were a replication external
	 * filter M program halting out after the other end of the pipe has been closed by the source server. In this case,
	 * the io_rundown call below would error out and would invoke the lastchance3 condition handler which will do an
	 * UNWIND that will return from gtm_exit_handler right away. To avoid an assert in the UNWIND macro (that checks
	 * we never do UNWINDs while process_exiting is set to TRUE) set a debug-only variable to TRUE. This variable is
	 * also checked by the assert in the UNWIND macro (see <C9K06_003278_test_failures/resolution_v2.txt> for details).
	 */
	assert(process_exiting);
	DEBUG_ONLY(ok_to_UNWIND_in_exit_handling = TRUE;)
	if (MUMPS_CALLIN & invocation_mode)
	{
		flush_pio();
		io_rundown(RUNDOWN_EXCEPT_STD);
	} else
		io_rundown(NORMAL_RUNDOWN);
	DEBUG_ONLY(ok_to_UNWIND_in_exit_handling = FALSE;)
	REVERT;

	print_exit_stats();
	if (need_core && !created_core && !dont_want_core)	/* We needed to core */
	{
		++core_in_progress;
		DUMP_CORE;		/* This will not return */
	}
}
