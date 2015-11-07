/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef INTERLOCK_H_INCLUDED
#define INTERLOCK_H_INCLUDED

#include "lockconst.h"
#include "compswap.h"
#include "aswp.h"

/* LATCH_{CLEAR,SET,CONFLICT} need to be in ascending order. wcs_* routines on this ordering for detecting out-of-range values */
#define LATCH_CLEAR	-1
#define LATCH_SET	0
#define LATCH_CONFLICT	1
#define	WRITE_LATCH_VAL(cr)	(cr)->interlock.semaphore	/* this can take either of the above 3 LATCH_* values */

#define INTERLOCK_INIT(X) ((X)->interlock.semaphore = LATCH_CLEAR, (X)->read_in_progress = -1)

#define INTERLOCK_INIT_MM(X) ((X)->interlock.semaphore = LATCH_CLEAR)

#define LOCK_NEW_BUFF_FOR_UPDATE(X) ((X)->interlock.semaphore = LATCH_SET)
#define LOCK_BUFF_FOR_UPDATE(X, Y, Z) (Y = adawi(1, &(X)->interlock.semaphore))
#define RELEASE_BUFF_UPDATE_LOCK(X, Y, Z) (Y = adawi(-1, &(X)->interlock.semaphore))
#define LOCK_BUFF_FOR_WRITE(X, Y) (Y = adawi(1, &(X)->interlock.semaphore))
#define CLEAR_BUFF_UPDATE_LOCK(X) (adawi(-1, &(X)->interlock.semaphore))

#define LOCK_BUFF_FOR_READ(X, Y) (Y = adawi(1, &(X)->read_in_progress))
#define RELEASE_BUFF_READ_LOCK(X) (adawi(-1, &(X)->read_in_progress))

#define WRITER_BLOCKED_BY_PROC(X) ((X) >= LATCH_SET)
#define WRITER_OWNS_BUFF(X) ((X) > LATCH_SET)
#define WRITER_STILL_OWNS_BUFF(X, Y) (Y = adawi(-1, &(X)->interlock.semaphore), (Y > LATCH_CLEAR))
#define OWN_BUFF(X) ((X) < LATCH_CONFLICT)

#define ADD_ENT_TO_ACTIVE_QUE_CNT(X, Y) (adawi(1, (X)))
#define SUB_ENT_FROM_ACTIVE_QUE_CNT(X, Y) (adawi(-1, (X)))

#define INCR_CNT(X, Y) (adawi(1, (X)))
#define DECR_CNT(X, Y) (adawi(-1, (X)))

#define GET_SWAPLOCK(X)			(COMPSWAP_LOCK((X), LOCK_AVAILABLE, 0, process_id, image_count))
/* Use COMPSWAP_UNLOCK to release the lock because of the memory barrier and other-processor notification it implies. Also
 * the usage of COMPSWAP_UNLOCK allows us to check (with low cost) that we have/had the lock we are trying to release.
 * If we don't have the lock and are trying to release it, a GTMASSERT seems the logical choice as the logic is very broken
 * at that point.  If this macro is used in part of an expression, the GTMASSERT path must also return a value (to keep
 * the compiler happy) thus the construct (GTMASSERT, 0) which returns a zero (see usage with assert() on UNIX).
 */
#define RELEASE_SWAPLOCK(X)		((COMPSWAP_UNLOCK((X), process_id, image_count, LOCK_AVAILABLE, 0)) ? 1 : (GTMASSERT, 0))

#endif /* INTERLOCK_H_INCLUDED */
