/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef DEBUG
#include "gtm_unistd.h"	/* for "getpid" */
#endif

#include "interlock.h"
#include "performcaslatchcheck.h"
#include "rel_quant.h"
#include "sleep_cnt.h"
#include "wcs_sleep.h"

GBLREF	int		num_additional_processors;
GBLREF	uint4		process_id;
GBLREF	volatile int4	fast_lock_count;		/* Stop interrupts while we have our parts exposed */

/* Grab a latch. If cannot get it in "max_retries" attempts, return FALSE, else TRUE.
 * Check for a max of 4 times whether holder pid is dead and if so salvage the lock.
 * Dont do it frequently as it involves is_proc_alive check which is a system call.
 */
boolean_t grab_latch(sm_global_latch_ptr_t latch, int max_timeout_in_secs)
{
	int	max_retries, retries, spins, maxspins, quarter_retries, next_cascheck, cursleep;

	assert(process_id == getpid());	/* make sure "process_id" global variable is reliable (used below in an assert) */
	if (process_id == latch->u.parts.latch_pid)
	{	/* already have lock */
		assert(FALSE);	/* dont expect caller to call us if we hold the lock already. in pro be safe and return */
		return TRUE;
	}
	++fast_lock_count;	/* Disable interrupts (i.e. wcs_stale) for duration to avoid potential deadlocks */
	/* Compute "max_retries" so total sleep time is "max_timeout_in_secs" seconds */
	quarter_retries = max_timeout_in_secs * MILLISECS_IN_SEC;
	DEBUG_ONLY(
		if (!quarter_retries)
			quarter_retries = 1;	/* dbg call to do grab_latch_immediate, reset to just 1 iteration */
	)
	max_retries = quarter_retries * 4; /* 1 loop in 4 is sleep of 1 msec */
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(max_retries, num_additional_processors) : 1;
	next_cascheck = max_retries - quarter_retries;
	cursleep = MINSLPTIME;
	for (retries = max_retries - 1; 0 < retries; retries--)	/* - 1 so do rel_quant 3 times first */
	{
		for (spins = maxspins; 0 < spins; spins--)
		{	/* We better not hold it if trying to get it */
			assert(latch->u.parts.latch_pid != process_id);
                        if (GET_SWAPLOCK(latch))
			{	/* Note that fast_lock_count is kept incremented for the duration that we hold the lock
				 * to prevent our dispatching an interrupt that could deadlock getting this lock
				 */
				--fast_lock_count;
				assert(0 <= fast_lock_count);
				return TRUE;
			}
		}
		/* Now that we have done a lot of spin, sleep a little. Do not use rel_quant as benchmarks done seem to
		 * suggest it is a more costly operation (system call + cpu overhead) in an environment with lots of processes.
		 */
		wcs_sleep(cursleep++);
		if (MAXSLPTIME == cursleep)
			cursleep = MINSLPTIME;	/* start all over again in sleep loop */
		/* For a total of 3 times in this function, see if target is dead and/or wake it up */
		if (retries == next_cascheck)
		{
			performCASLatchCheck(latch, TRUE);
			next_cascheck -= quarter_retries;
		}
	}
	--fast_lock_count;
	assert(0 <= fast_lock_count);
	assert(FALSE);
	return FALSE;
}
