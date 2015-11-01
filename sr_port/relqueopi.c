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

#include "aswp.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "lockconst.h"
#include "copy.h"
#include "interlock.h"
#include "relqueopi.h"
#include "performcaslatchcheck.h"
#include "relqop.h"
#include "wcs_backoff.h"
#include "caller_id.h"

GBLREF	volatile	int4	fast_lock_count;
GBLREF	int4		process_id;
GBLREF	gd_region	*gv_cur_region;
GBLREF	int		num_additional_processors;

#define QI_RETRY	128

#ifdef QI_STARVATION
#  undef QI_STARVATION
#endif

#define QI_STARVATION	1000

#ifdef DEBUG_QUEUE
#define VERIFY_QUEUE(base) verify_queue(base)
#else
#define VERIFY_QUEUE(base)
#endif


int insqhi2(que_ent_ptr_t new, que_head_ptr_t base)
{
	int	retries, spins, maxspin;

	++fast_lock_count;			/* Disable wcs_stale for duration */
	maxspin = num_additional_processors ? QI_RETRY : 1;
	for (retries = 0 ;  retries < QI_STARVATION ;  retries++)
	{
		for (spins = maxspin; 0 < spins; spins--)
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
		if (0 != retries)
			wcs_backoff(retries);
		performCASLatchCheck(&base->latch, retries);
	}
	DUMP_LOCKHIST();
	--fast_lock_count;
	assert(0 <= fast_lock_count);
	assert(FALSE);
	return INTERLOCK_FAIL;
}


int insqti2(que_ent_ptr_t new, que_head_ptr_t base)
{
	int	retries, spins, maxspin;

	++fast_lock_count;			/* Disable wcs_stale for duration */
	maxspin = num_additional_processors ? QI_RETRY : 1;
	for (retries = 0 ;  retries < QI_STARVATION ;  retries++)
	{
		for (spins = maxspin; 0 < spins; spins--)
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
		if (0 != retries)
			wcs_backoff(retries);
		performCASLatchCheck(&base->latch, retries);
	}
	DUMP_LOCKHIST();
	--fast_lock_count;
	assert(0 <= fast_lock_count);
	assert(FALSE);
	return INTERLOCK_FAIL;
}


que_ent_ptr_t remqhi1(que_head_ptr_t base)
{
	int		retries, spins, maxspin;
	que_ent_ptr_t	ret;

	++fast_lock_count;			/* Disable wcs_stale for duration */
	maxspin = num_additional_processors ? QI_RETRY : 1;
	for (retries = 0 ;  retries < QI_STARVATION ;  retries++)
	{
		for (spins = maxspin; 0 < spins; spins--)
		{
                        if (GET_SWAPLOCK(&base->latch))
			{
				LOCK_HIST("OBTN", &base->latch, process_id, retries);
				VERIFY_QUEUE(base);
				ret = (que_ent_ptr_t)remqh((que_ent_ptr_t)base);
				VERIFY_QUEUE(base);
				LOCK_HIST("RLSE", &base->latch, process_id, retries);
				if (NULL != ret)
					ret->fl = ret->bl = 0;
                                RELEASE_SWAPLOCK(&base->latch);
				--fast_lock_count;
				assert(0 <= fast_lock_count);
				return ret;
			}
		}
		if (0 != retries)
			wcs_backoff(retries);
		performCASLatchCheck(&base->latch, retries);
	}
	DUMP_LOCKHIST();
	--fast_lock_count;
	assert(0 <= fast_lock_count);
	assert(FALSE);
	return (que_ent_ptr_t)INTERLOCK_FAIL;
}


que_ent_ptr_t remqti1(que_head_ptr_t base)
{
	int		retries, spins, maxspin;
	que_ent_ptr_t	ret;

	++fast_lock_count;			/* Disable wcs_stale for duration */
	maxspin = num_additional_processors ? QI_RETRY : 1;
	for (retries = 0 ;  retries < QI_STARVATION ;  retries++)
	{
		for (spins = maxspin; 0 < spins; spins--)
		{
                        if (GET_SWAPLOCK(&base->latch))
			{
				LOCK_HIST("OBTN", &base->latch, process_id, retries);
				VERIFY_QUEUE(base);
				ret = (que_ent_ptr_t)remqt((que_ent_ptr_t)base);
				VERIFY_QUEUE(base);
				LOCK_HIST("RLSE", &base->latch, process_id, retries);
				if (NULL != ret)
					ret->fl = ret->bl = 0;
                                RELEASE_SWAPLOCK(&base->latch);
				--fast_lock_count;
				assert(0 <= fast_lock_count);
				return ret;
			}
		}
		if (0 != retries)
			wcs_backoff(retries);
		performCASLatchCheck(&base->latch, retries);
	}
	DUMP_LOCKHIST();
	--fast_lock_count;
	assert(0 <= fast_lock_count);
	assert(FALSE);
	return (que_ent_ptr_t)INTERLOCK_FAIL;
}
#endif
