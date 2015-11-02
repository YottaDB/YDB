/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	relqueopi - C-callable relative queue interlocked routines
 *
 *	These routines perform interlocked operations on doubly-linked
 *	relative queues.  They are designed to emulate the VAX machine
 *	instructions (and corresponding VAX C library routines) after
 *	which they are named.
 *
 *	insqhi - insert entry into queue at head, interlocked
 *	insqti - insert entry into queue at tail, interlocked
 *	remqhi - remove entry from queue at head, interlocked
 *	remqti - remove entry from queue at tail, interlocked
 */

#include "mdef.h"

/* N.B. lockconst.h is needed for the lock values.
 *	all the rest are needed to define INTERLOCK_FAIL (sigh).
 */

/* These are instructions/library routines on VMS so this module is unnecessary for those platforms. */
#ifndef VMS

#include <errno.h>
#include "aswp.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "copy.h"
#include "interlock.h"
#include "relqueopi.h"
#include "performcaslatchcheck.h"
#include "relqop.h"
#include "wcs_sleep.h"
#include "caller_id.h"
#include "rel_quant.h"
#include "sleep_cnt.h"
#include "gtm_c_stack_trace.h"

GBLREF	volatile	int4	fast_lock_count;
GBLREF	int4		process_id;
GBLREF	gd_region	*gv_cur_region;
GBLREF	int		num_additional_processors;

int insqhi2(que_ent_ptr_t new, que_head_ptr_t base)
{
	int	retries, spins, maxspins;
	uint4	stuck_cnt = 0;

	++fast_lock_count;			/* Disable wcs_stale for duration */
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	for (retries = LOCK_TRIES - 1; 0 < retries; retries--)	/* - 1 so do rel_quant 3 times first */
	{
		for (spins = maxspins; 0 < spins; spins--)
		{
                        if (GET_SWAPLOCK(&base->latch))
			{
				LOCK_HIST("OBTN", &base->latch, process_id, retries);
				VERIFY_QUEUE(base);
				insqh(new, (que_ent_ptr_t)base);
				VERIFY_QUEUE(base);
				LOCK_HIST("RLSE", &base->latch, process_id, retries);
                                RELEASE_SWAPLOCK(&base->latch);
				--fast_lock_count;
				assert(0 <= fast_lock_count);
				return QUEUE_INSERT_SUCCESS;
			}
		}
		if (retries & 0x3)
			/* On all but every 4th pass, do a simple rel_quant */
			rel_quant();	/* Release processor to holder of lock (hopefully) */
		else
		{
			/* On every 4th pass, we bide for awhile */
			wcs_sleep(LOCK_SLEEP);
			assert(0 == (LOCK_TRIES % 4)); /* assures there are 3 rel_quants prior to first wcs_sleep() */
			/* If near end of loop, see if target is dead and/or wake it up */
			if (RETRY_CASLATCH_CUTOFF == retries)
				performCASLatchCheck(&base->latch, TRUE);
		}
	}
	DUMP_LOCKHIST();
	--fast_lock_count;
	stuck_cnt++;
	GET_C_STACK_FROM_SCRIPT("INTERLOCK_FAIL", process_id, base->latch.u.parts.latch_pid, stuck_cnt);
	assert(0 <= fast_lock_count);
	assert(FALSE);
	return INTERLOCK_FAIL;
}


int insqti2(que_ent_ptr_t new, que_head_ptr_t base)
{
	int	retries, spins, maxspins;
	uint4	stuck_cnt = 0;

	++fast_lock_count;			/* Disable wcs_stale for duration */
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	for (retries = LOCK_TRIES - 1; 0 < retries; retries--)	/* - 1 so do rel_quant 3 times first */
	{
		for (spins = maxspins; 0 < spins; spins--)
		{
                        if (GET_SWAPLOCK(&base->latch))
			{
				LOCK_HIST("OBTN", &base->latch, process_id, retries);
				VERIFY_QUEUE(base);
				insqt(new, (que_ent_ptr_t)base);
				VERIFY_QUEUE(base);
				LOCK_HIST("RLSE", &base->latch, process_id, retries);
                                RELEASE_SWAPLOCK(&base->latch);
				--fast_lock_count;
				assert(0 <= fast_lock_count);
				return QUEUE_INSERT_SUCCESS;
			}
		}
		if (retries & 0x3)
			/* On all but every 4th pass, do a simple rel_quant */
			rel_quant();	/* Release processor to holder of lock (hopefully) */
		else
		{
			/* On every 4th pass, we bide for awhile */
			wcs_sleep(LOCK_SLEEP);
			/* If near end of loop, see if target is dead and/or wake it up */
			assert(0 == (LOCK_TRIES % 4)); /* assures there are 3 rel_quants prior to first wcs_sleep() */
			if (RETRY_CASLATCH_CUTOFF == retries)
				performCASLatchCheck(&base->latch, TRUE);
		}
	}
	DUMP_LOCKHIST();
	--fast_lock_count;
	stuck_cnt++;
	GET_C_STACK_FROM_SCRIPT("INTERLOCK_FAIL", process_id, base->latch.u.parts.latch_pid, stuck_cnt);
	assert(0 <= fast_lock_count);
	assert(FALSE);
	return INTERLOCK_FAIL;
}


void_ptr_t remqhi1(que_head_ptr_t base)
{
	int		retries, spins, maxspins;
	que_ent_ptr_t	ret;
	uint4		stuck_cnt = 0;

	++fast_lock_count;			/* Disable wcs_stale for duration */
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	for (retries = LOCK_TRIES - 1; 0 < retries; retries--)	/* - 1 so do rel_quant 3 times first */
	{
		for (spins = maxspins; 0 < spins; spins--)
		{
                        if (GET_SWAPLOCK(&base->latch))
			{
				LOCK_HIST("OBTN", &base->latch, process_id, retries);
				VERIFY_QUEUE(base);
				ret = (que_ent_ptr_t)remqh((que_ent_ptr_t)base);
				VERIFY_QUEUE(base);
				LOCK_HIST("RLSE", &base->latch, process_id, retries);
				/* Unix wcs_get_space and the queue functions in gtm_relqueopi.c rely on the links being
				 * reset to 0 right after an element is removed from the queue. Note that this has to be
				 * done BEFORE releasing the queue header lock as Unix wcs_get_space assumes its has
				 * exclusive control of the queue if it has the queue header lock.
				 */
				if (NULL != ret)
					ret->fl = ret->bl = 0;
                                RELEASE_SWAPLOCK(&base->latch);
				--fast_lock_count;
				assert(0 <= fast_lock_count);
				return (void_ptr_t)ret;
			}
		}
		if (retries & 0x3)
			/* On all but every 4th pass, do a simple rel_quant */
			rel_quant();	/* Release processor to holder of lock (hopefully) */
		else
		{
			/* On every 4th pass, we bide for awhile */
			wcs_sleep(LOCK_SLEEP);
			/* If near end of loop, see if target is dead and/or wake it up */
			assert(0 == (LOCK_TRIES % 4)); /* assures there are 3 rel_quants prior to first wcs_sleep() */
			if (RETRY_CASLATCH_CUTOFF == retries)
				performCASLatchCheck(&base->latch, TRUE);
		}
	}
	DUMP_LOCKHIST();
	--fast_lock_count;
	stuck_cnt++;
	GET_C_STACK_FROM_SCRIPT("INTERLOCK_FAIL", process_id, base->latch.u.parts.latch_pid, stuck_cnt);
	assert(0 <= fast_lock_count);
	assert(FALSE);
	return (void_ptr_t)INTERLOCK_FAIL;
}


void_ptr_t remqti1(que_head_ptr_t base)
{
	int		retries, spins, maxspins;
	que_ent_ptr_t	ret;
	uint4		stuck_cnt = 0;

	++fast_lock_count;			/* Disable wcs_stale for duration */
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	for (retries = LOCK_TRIES - 1; 0 < retries; retries--)	/* - 1 so do rel_quant 3 times first */
	{
		for (spins = maxspins; 0 < spins; spins--)
		{
                        if (GET_SWAPLOCK(&base->latch))
			{
				LOCK_HIST("OBTN", &base->latch, process_id, retries);
				VERIFY_QUEUE(base);
				ret = (que_ent_ptr_t)remqt((que_ent_ptr_t)base);
				VERIFY_QUEUE(base);
				LOCK_HIST("RLSE", &base->latch, process_id, retries);
				/* Unix wcs_get_space and the queue functions in gtm_relqueopi.c rely on the links being
				 * reset to 0 right after an element is removed from the queue. Note that this has to be
				 * done BEFORE releasing the queue header lock as Unix wcs_get_space assumes its has
				 * exclusive control of the queue if it has the queue header lock.
				 */
				if (NULL != ret)
					ret->fl = ret->bl = 0;
                                RELEASE_SWAPLOCK(&base->latch);
				--fast_lock_count;
				assert(0 <= fast_lock_count);
				return (void_ptr_t)ret;
			}
		}
		if (retries & 0x3)
			/* On all but every 4th pass, do a simple rel_quant */
			rel_quant();	/* Release processor to holder of lock (hopefully) */
		else
		{
			/* On every 4th pass, we bide for awhile */
			wcs_sleep(LOCK_SLEEP);
			assert(0 == (LOCK_TRIES % 4)); /* assures there are 3 rel_quants prior to first wcs_sleep() */
			/* If near end of loop, see if target is dead and/or wake it up */
			if (RETRY_CASLATCH_CUTOFF == retries)
				performCASLatchCheck(&base->latch, TRUE);
		}
	}
	DUMP_LOCKHIST();
	--fast_lock_count;
	stuck_cnt++;
	GET_C_STACK_FROM_SCRIPT("INTERLOCK_FAIL", process_id, base->latch.u.parts.latch_pid, stuck_cnt);
	assert(0 <= fast_lock_count);
	assert(FALSE);
	return (void_ptr_t)INTERLOCK_FAIL;
}
#endif
