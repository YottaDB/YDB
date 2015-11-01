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

#define GET_SWAPLOCK(X)			(compswap((X), LOCK_AVAILABLE, process_id))
/* Use ASWP to release the lock because of the memory barrier and other-processor notification it implies */
#define RELEASE_SWAPLOCK(X)		{									\
	                                        assert((X)->latch_pid == process_id);				\
						ASWP(((sm_int_ptr_t)&(X)->latch_pid), LOCK_AVAILABLE, (X));	\
                                        }
