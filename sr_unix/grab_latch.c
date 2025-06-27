/****************************************************************
 *								*
 * Copyright (c) 2014-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

#include "gtm_facility.h"
#include "interlock.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "performcaslatchcheck.h"
#include "gtm_rel_quant.h"
#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "min_max.h"
#include "gt_timer.h"

GBLREF	int		num_additional_processors, process_exiting;
GBLREF	uint4		process_id;
GBLREF	volatile int4	fast_lock_count;		/* Stop interrupts while we have our parts exposed */

/* Grab a latch. If cannot get it in the approximate time requested, return FALSE, else TRUE.
 * originally intended to protect verify_queue which only runs in debug
 * but adopted by some auto-relink code - not clear that's appropriate
 */
boolean_t grab_latch(sm_global_latch_ptr_t latch, int max_timeout_in_secs, wait_state state, sgmnt_addrs *csa)
{
	ABS_TIME	cur_time, end_time, remain_time;
	time_t		last_cas_latch_check_remain_secs = 0;
	int		maxspins, retries, spins;
	int4		max_sleep_mask;
	boolean_t	skip_time_calc;

	assert(process_id == getpid());	/* Make sure "process_id" global variable is reliable (used below in an assert) */
	if (process_id == latch->u.parts.latch_pid)
	{	/* Already have lock - this shouldn't normally happen but can happen if a fatal interrupt catches us just right
		 * so pro we always play safe and return but with a debug build, allow only process_exiting to continue. This
		 * should normally not be a problem since we won't be returning to the code that grabbed the lock if we are
		 * exiting.
		 */
		assert(process_exiting);
		return TRUE;
	}
	skip_time_calc = !max_timeout_in_secs || (GRAB_LATCH_INDEFINITE_WAIT == max_timeout_in_secs);
	if (!skip_time_calc)
	{
		sys_get_curr_time(&cur_time);
		add_int_to_abs_time(&cur_time, max_timeout_in_secs * 1000, &end_time);
		remain_time.tv_sec = 0;		/* ensure one try */
		last_cas_latch_check_remain_secs = 0;
	}
	/* Define number of hard-spins the inner loop does */
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	if (NULL != csa)
		UPDATE_CRIT_COUNTER(csa, state);
	for (retries = 1; ; retries++)
	{
		++fast_lock_count;	/* Disable interrupts (i.e. wcs_stale) for duration to avoid potential deadlocks */
		for (spins = maxspins; spins > 0 ; spins--)
		{	/* We better not hold it if trying to get it */
			assert(latch->u.parts.latch_pid != process_id);
			if (GET_SWAPLOCK(latch))
			{
				--fast_lock_count;
				assert(0 <= fast_lock_count);
				return TRUE;
			}
		}
		--fast_lock_count;
		if (!max_timeout_in_secs)
			break;
		if (!skip_time_calc)
		{
			/* The below code is similar to the REST_FOR_LATCH macro except that the "performCASLatchCheck()" call
			 * is based on elapsed time (available in this function) instead of iterations (in the generic macro).
			 */
			if (0 == (retries & LOCK_SPIN_HARD_MASK))
				GTM_REL_QUANT(USEC_IN_NSEC_MASK);	/* Release processor to holder of lock (hopefully) */
			sys_get_curr_time(&cur_time);
			remain_time = sub_abs_time(&end_time, &cur_time);
			if (0 > remain_time.tv_sec)
				break;

			/* Since we are anyways recording remaining time, use this chance to do dead pid check every 1 second */
			if (last_cas_latch_check_remain_secs && (remain_time.tv_sec != last_cas_latch_check_remain_secs))
			{	/* It has been at least 1 second since we last did a dead pid check.
				 * Check if we're due to check for lock abandonment check or holder wakeup.
				 */
				performCASLatchCheck(latch, TRUE);
			}
			last_cas_latch_check_remain_secs = remain_time.tv_sec;
		} else
		{	/* Indefinite wait for lock. Periodically check if latch is held by dead pid. If so get it back. */
			SLEEP_FOR_LATCH(latch, retries);
		}
	}
	assert(0 <= fast_lock_count);
	assert(FALSE);
	return FALSE;
}
