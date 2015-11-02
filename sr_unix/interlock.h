/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* note -- this may still need cleaning up for multiprocessor implementations, may need a get_latch */

#include "lockconst.h"
#include "compswap.h"
#include "aswp.h"

/* LATCH_{CLEAR,SET,CONFLICT} need to be in ascending order. wcs_verify relies on this ordering for detecting out-of-range values */
#define LATCH_CLEAR	0
#define LATCH_SET	1
#define LATCH_CONFLICT	2
#define	WRITE_LATCH_VAL(cr)		(cr)->interlock.latch	/* this can take either of the above 3 LATCH_* values */

#define INTERLOCK_INIT(X)		{SET_LATCH((sm_int_ptr_t)&((X)->interlock.latch), LATCH_CLEAR); \
					 (X)->read_in_progress = -1;	\
					 SET_LATCH_GLOBAL(&((X)->rip_latch), LOCK_AVAILABLE);}
	/* On HPPA, SET_LATCH_GLOBAL forces its available value, for others it is the same as SET_LATCH */

#define INTERLOCK_INIT_MM(X)		(SET_LATCH((sm_int_ptr_t)&((X)->interlock.latch), LATCH_CLEAR))
					  /* similar to INTERLOCK_INIT except this is for a mmblk_rec */

/* New buffer doesn't need interlocked operation.  */
#define LOCK_NEW_BUFF_FOR_UPDATE(X)	(SET_LATCH((sm_int_ptr_t)&((X)->interlock.latch), LATCH_SET))

#define LOCK_BUFF_FOR_UPDATE(X,Y,Z)     {Y = ASWP((sm_int_ptr_t)&(X)->interlock.latch, LATCH_SET, Z); \
						 LOCK_HIST("OBTN", &(X)->interlock.latch, process_id, -1);}
#define RELEASE_BUFF_UPDATE_LOCK(X,Y,Z) {LOCK_HIST("RLSE", &(X)->interlock.latch, process_id, -1); \
					         Y = ASWP((sm_int_ptr_t)&(X)->interlock.latch, LATCH_CLEAR, Z);}
#define CLEAR_BUFF_UPDATE_LOCK(X,Z)     (ASWP((sm_int_ptr_t)&(X)->interlock.latch, LATCH_CLEAR, Z))

#define LOCK_BUFF_FOR_READ(X,Y)		(Y = INCR_CNT((sm_int_ptr_t)&(X)->read_in_progress, \
						      (sm_global_latch_ptr_t)&(X)->rip_latch))
#define RELEASE_BUFF_READ_LOCK(X)	(DECR_CNT((sm_int_ptr_t)&(X)->read_in_progress, \
						  (sm_global_latch_ptr_t)&(X)->rip_latch))

/* Only the writer uses the LATCH_CONFLICT value.  */
#define LOCK_BUFF_FOR_WRITE(X,Y,Z)      (Y = ASWP((sm_int_ptr_t)&(X)->interlock.latch, LATCH_CONFLICT, Z))

/* Used after an aswp; if the return value (second argument to one of these macros) was
 * LATCH_CONFLICT, then the writer owned the buffer when this process performed the aswp.
 * WRITER_BLOCKED_BY_PROC is used by a non-writer process.
 * WRITER_OWNS_BUFF is used by the writer.
 */
#define WRITER_BLOCKED_BY_PROC(X)	((X) == LATCH_CONFLICT)
#define WRITER_OWNS_BUFF(X)		((X) == LATCH_CONFLICT)

#define WRITER_STILL_OWNS_BUFF(X,Y)	((X)->interlock.latch != LATCH_CLEAR)

/* Used after an aswp; if the return value (second argument to one of these macros)
 * was LATCH_CLEAR, then nobody had it when this process performed the aswp so this
 * process owns the resource.
 */
#define OWN_BUFF(X)			((X) == LATCH_CLEAR)

#define ADD_ENT_TO_ACTIVE_QUE_CNT(X,Y)		(INCR_CNT((sm_int_ptr_t)(X), (sm_global_latch_ptr_t)(Y)))
#define SUB_ENT_FROM_ACTIVE_QUE_CNT(X,Y)	(DECR_CNT((sm_int_ptr_t)(X), (sm_global_latch_ptr_t)(Y)))

#define INCR_CNT(X,Y)			INTERLOCK_ADD(X,Y,1)
#define DECR_CNT(X,Y)			INTERLOCK_ADD(X,Y,-1)

#define BIT0_SETI(X,Z)                  (ASWP((sm_int_ptr_t)(X),1,Z))
#define BIT0_CLEARI(X,Z)                (!ASWP((sm_int_ptr_t)(X),0,Z))

#define IS_BIT0_SET(X)			((X) == 1)
#define IS_BIT0_CLEAR(X)		((X) == 0)

#ifndef __ia64
#define GET_SWAPLOCK(X)			(COMPSWAP((X), LOCK_AVAILABLE, 0, process_id, 0))
#else
#define GET_SWAPLOCK(X)			(COMPSWAP_LOCK((X), LOCK_AVAILABLE, 0, process_id, 0))
#endif /* __ia64 */
/* Use COMPSWAP to release the lock because of the memory barrier and other-processor notification it implies. Also
   the usage of COMPSWAP allows us to check (with low cost) that we have/had the lock we are trying to release. If we
   don't have the lock and are trying to release it, a GTMASSERT seems the logical choice as the logic is very broken
   at that point. If this macro is used in part of an expression, the GTMASSERT path must also return a value (to keep
   the compiler happy) thus the construct (GTMASSERT, 0) which returns a zero (see usage with assert() on UNIX).
 */
#ifndef __ia64
#define RELEASE_SWAPLOCK(X)		(COMPSWAP((X), process_id, 0, LOCK_AVAILABLE, 0) ? 1 : (GTMASSERT, 0))
#else
#define RELEASE_SWAPLOCK(X)		(COMPSWAP_UNLOCK((X), process_id, 0, LOCK_AVAILABLE, 0) ? 1 : (GTMASSERT, 0))
#endif /* __ia64 */

