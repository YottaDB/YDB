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

GBLREF	volatile int4	fast_lock_count;		/* Stop interrupts while we have our parts exposed */
GBLREF	uint4		process_id;

/* Grab a latch. If cannot get it, return FALSE, else TRUE. */
void	rel_latch(sm_global_latch_ptr_t latch)
{
	++fast_lock_count;	/* Disable interrupts (i.e. wcs_stale) for duration to avoid potential deadlocks */
	assert(process_id == getpid());	/* make sure "process_id" global variable is reliable (used below in RELEASE_SWAPLOCK) */
	RELEASE_SWAPLOCK(latch);
	--fast_lock_count;
	assert(0 <= fast_lock_count);
}
