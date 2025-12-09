/****************************************************************
 *								*
 * Copyright (c) 2008-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

#include "gtm_stdlib.h"
#include "gtm_signal.h"
/* Note the use of __GLIBC__ here precludes this include on CYGWIN and Alpine(MUSL environments */
#ifdef __GLIBC__
#include <execinfo.h>
#endif

#include "gt_timer.h"
#include "caller_id.h"
#include <execinfo.h>
#include <dlfcn.h>
#include <string.h>
#include "util.h"
#include "min_max.h"

/* the list trace[MAX_TRACE_DEPTH] must be sufficient to be indexed at
 * trace_start + extra_frames + RETURN_ADDRESS_DEPTH.
 * So based on the assumptions that the maximum extra_frames value is 8,
 * +2 for RETURN_ADDRESS_DEPTH, +1 for max trace_start, +1 for base 0 index
 * +2 for extra padding MAX_TRACE_DEPTH=14.
 * Link to origin of the max value of 8 and more details.
 * https://gitlab.com/YottaDB/DB/YDB/-/merge_requests/1799#note_3069976924
 */
#define MAX_TRACE_DEPTH		14
/* We need the callers caller of caller_id */
#define RETURN_ADDRESS_DEPTH	2

GBLREF	boolean_t		blocksig_initialized;
GBLREF	sigset_t		block_sigsent;
GBLREF	int			process_exiting;
GBLREF	volatile boolean_t	timer_in_handler;

static boolean_t caller_id_reent = FALSE;	/* If ever true, our helper gets a lobotomy */

caddr_t caller_id(unsigned int extra_frames)
{
	void		*trace[MAX_TRACE_DEPTH], *to_ret;
	int		rc, trace_size, trace_start, trace_target, dladdr_status;
	sigset_t	savemask;
	Dl_info		backtrace_ptr_info;

	/* We cannot let this routine nest itself due to the impolite things that
	 * occur when the exception routines get re-entered so just play dead.
	 */
	if (caller_id_reent)
		return (caddr_t)-1;
	/* Neither Cygwin (newlib based libc) or Alpine (musl libc) support backtrace() which is a GNU extension */
	#ifndef __GLIBC__
	return NULL;
	#endif
	/* Return 0 if we are already in timer or generic signal signal handler to prevent deadlocks
	 * due to nested mallocs/frees resulting from interrupting in external function calls.
	 */
	if (timer_in_handler || process_exiting)
		return (caddr_t)0;
	/* When backtrace is processing and a signal occurs, there is the potential for a deadlock -- waiting on a
	 * lock that this process already holds. A work around is to temporarily block signals (SIGINT, SIGQUIT,
	 * SIGTERM, SIGTSTP, SIGCONT, SIGALRM) and then restore them after the backtrace call returns.
	 */
	/* It is possible for this routine to be invoked during process startup (as part of err_init()) so
	 * block_sigsent could not be initialized yet. Therefore this does not have an assert(blocksig_initialized)
	 * that similar code in other places (e.g. dollarh.c) has.
	 */
	if (blocksig_initialized)
		SIGPROCMASK(SIG_BLOCK, &block_sigsent, &savemask, rc);
	caller_id_reent = TRUE;
	/* The trace will be indexed at extra_frames + RETURN_ADDRESS_DEPTH (possibly + 1, see trace_start below)
	 * So if extra_frames + RETURN_ADDRESS_DEPTH + 1 >= MAX_TRACE_DEPTH, then the array will be indexed out of bounds.
	 */
	assert(MAX_TRACE_DEPTH > (extra_frames + RETURN_ADDRESS_DEPTH));
	/* Some systems add the backtrace function to the backtrace and some do not.
	 * To account for this, set trace_start to be one if a non-libyottadb.so function is at the start of the backtrace.
	 * Thus, an extra function added to the backtrace can be ignored by indexing starting from the value of trace_start.
	 */
	trace_size = backtrace(trace, MIN(2 + extra_frames + RETURN_ADDRESS_DEPTH, MAX_TRACE_DEPTH));
	if (0 < trace_size)
	{
		dladdr_status = dladdr(trace[0], &backtrace_ptr_info);
		trace_start = dladdr_status && (NULL == strstr(backtrace_ptr_info.dli_fname, "libyottadb.so"));
		trace_target = trace_start + extra_frames + RETURN_ADDRESS_DEPTH;
		/* Assert sufficient room in trace for backtrace. */
		assert((trace_target + 1) <= MAX_TRACE_DEPTH);
		/* Should only be possible for this assert to fail if extra_frames is large enough
		* that backtrace is attempting to look back further than the size of the stack. (extra_frames+2 >= stack_size)
		*/
		assert(trace_target < trace_size);
		if (trace_target >= trace_size)
			to_ret = trace[trace_size - 1]; /* If not enough frames exist, return the last one. */
		else
			to_ret = trace[trace_target];
	} else
		to_ret = NULL;
	caller_id_reent = FALSE;
	if (blocksig_initialized)
		SIGPROCMASK(SIG_SETMASK, &savemask, NULL, rc);
	/* Backtrace will return call stack with address */
	return (caddr_t)to_ret;
}
