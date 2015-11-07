/****************************************************************
 *								*
 * Copyright (c) 2014-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "min_max.h"

GBLREF	int		num_additional_processors;
GBLREF	uint4		process_id;
GBLREF	volatile int4	fast_lock_count;		/* Stop interrupts while we have our parts exposed */

/* Grab a latch. If cannot get it in the approximate time requested, return FALSE, else TRUE.
 */
boolean_t grab_latch(sm_global_latch_ptr_t latch, int max_timeout_in_secs)
{
	int	max_retries, retries, spins, maxspins;

	assert(process_id == getpid());	/* Make sure "process_id" global variable is reliable (used below in an assert) */
	if (process_id == latch->u.parts.latch_pid)
	{	/* Already have lock */
		assert(FALSE);	/* Don't expect caller to call us if we hold the lock already. in pro be safe and return */
		return TRUE;
	}
	++fast_lock_count;	/* Disable interrupts (i.e. wcs_stale) for duration to avoid potential deadlocks */
	/* Compute "max_retries" so total sleep time is "max_timeout_in_secs" seconds */
	max_retries = max_timeout_in_secs * LOCK_TRIES_PER_SEC;
	/* Some DEBUG build calls have 0 timeout so want just one iteration but since we subtract one from the max to
	 * avoid sleeping the first round, make it 2.
	 */
	DEBUG_ONLY(max_retries = MAX(max_retries, 2));
	/* Define number of hard-spins the inner loop does */
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	for (retries = max_retries - 1; 0 < retries; retries--)	/* Subtract 1 so don't do sleep till 3rd pass */
	{
		for (spins = maxspins; 0 < spins; spins--)
		{	/* We better not hold it if trying to get it */
			assert(latch->u.parts.latch_pid != process_id);
                        if (GET_SWAPLOCK(latch))
			{
				--fast_lock_count;
				assert(0 <= fast_lock_count);
				return TRUE;
			}
		}
		if (retries & 0x3)
			/* On all but every 4th pass, do a simple rel_quant */
			rel_quant();	/* Release processor to holder of lock (hopefully) */
		else
		{
			/* On every 4th pass, take a cat-nap */
			wcs_sleep(LOCK_SLEEP);
			/* Check if we're due to check for lock abandonment check or holder wakeup */
			if (0 == (retries & (LOCK_CASLATCH_CHKINTVL - 1)))
				performCASLatchCheck(latch, TRUE);
		}
	}
	--fast_lock_count;
	assert(0 <= fast_lock_count);
	assert(FALSE);
	return FALSE;
}
