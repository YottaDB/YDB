/****************************************************************
 *								*
 *	Copyright 2008, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* caller_id routine called from CRIT_TRACE macro to
 * return the return address of our caller allowing CRIT_TRACE
 * (used in various semaphore routines) to determine who was
 * calling those semaphore routines and for what purpose and
 * when. This is a system dependent type of operation and is
 * generally implemented in assembly language.
 * Presently 32bit linux system has its own implementation in
 * assembly. Similar implementation will not work on x86_64
 * since register rbp is also used as M Frame pointer in its
 * assembly files.
 * This particular implementation will work only on Linux x86_64 system
 * due to its dependency on "backtrace" function call which is not
 * available on all Unix flovours.
 */

#include "mdef.h"

#include <execinfo.h>
#include <signal.h>

#include "gtm_stdlib.h"
#include "gt_timer.h"
#include "caller_id.h"

#define MAX_TRACE_DEPTH		3
/*We need the callers caller of caller_id */
#define RETURN_ADDRESS_DEPTH	2

GBLREF	boolean_t		blocksig_initialized;
GBLREF	sigset_t		block_sigsent;
GBLREF	int			process_exiting;
GBLREF	volatile boolean_t	timer_in_handler;

static boolean_t caller_id_reent = FALSE;	/* If ever true, our helper gets a lobotomy */

caddr_t caller_id(void)
{
	void *trace[MAX_TRACE_DEPTH];
	int trace_size;
	sigset_t	savemask;

	/* We cannot let this routine nest itself due to the impolite things that
	 * occur when the exception routines get re-entered so just play dead.
	 */
	if (caller_id_reent)
		return (caddr_t)-1;
	/* Return 0 if we are already in timer or generic signal signal handler to prevent deadlocks
	 * due to nested mallocs/frees resulting from interrupting in external function calls.
	 */
	if (timer_in_handler || process_exiting)
		return (caddr_t)0;
	/* When backtrace is processing and a signal occurs, there is the potential for a deadlock -- waiting on a
	 * lock that this process already holds.  A work around is to temporarily block signals (SIGINT, SIGQUIT,
	 * SIGTERM, SIGTSTP, SIGCONT, SIGALRM) and then restore them after the backtrace call returns.
	 */
	/* It is possible for this routine to be invoked during process startup (as part of err_init()) so
	 * block_sigsent could not be initialized yet. Therefore this does not have an assert(blocksig_initialized)
	 * that similar code in other places (e.g. dollarh.c) has.
	 */
	if (blocksig_initialized)
        	sigprocmask(SIG_BLOCK, &block_sigsent, &savemask);
	caller_id_reent = TRUE;
	trace_size = backtrace(trace, MAX_TRACE_DEPTH);
	caller_id_reent = FALSE;
	if (blocksig_initialized)
		sigprocmask(SIG_SETMASK, &savemask, NULL);
	/* backtrace will return call stack with address.*/
	if (trace_size >= RETURN_ADDRESS_DEPTH)
		return (caddr_t)trace[RETURN_ADDRESS_DEPTH];
	else
		return NULL;
}
