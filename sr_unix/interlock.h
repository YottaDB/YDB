/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "memcoherency.h"

#ifdef __x86_64__
#	include <immintrin.h>		/* needed only with ydb mutex (not with pthread mutex) */
#elif defined(__aarch64__)
/* #	include <arm_acle.h> */		/* needed if we were using __yield() ACLE intrinsic in the SPINLOCK_PAUSE macro below.
					 * But that does not work (see macro definition below). So this #include is commented now.
					 */
#else
#	error UNSUPPORTED PLATFORM
#endif

/* Define memory barrier macro to use in LOCK_BUFF_FOR_READ/RELEASE_BUFF_READ_LOCK macros below. On AIX
 * the locking macros have no memory barrier connotation because they use load/store_locked instructions.
 * The macros defined below give us that connotation on the platforms that need it and avoid adding additional
 * memory barriers on platforms that already have them.
 */
#if defined(_AIX)
#  define READ_LOCK_READ_MEMBARRIER SHM_READ_MEMORY_BARRIER
#  define READ_LOCK_WRITE_MEMBARRIER SHM_WRITE_MEMORY_BARRIER
#else
#  define READ_LOCK_READ_MEMBARRIER
#  define READ_LOCK_WRITE_MEMBARRIER
#endif

/* LATCH_{CLEAR,SET,CONFLICT} need to be in ascending order. wcs_verify relies on this ordering for detecting out-of-range values */
#define LATCH_CLEAR	0
#define LATCH_SET	1
#define LATCH_CONFLICT	2
#define	WRITE_LATCH_VAL(cr)		(cr)->interlock.latch	/* this can take either of the above 3 LATCH_* values */

#define INTERLOCK_INIT(X)											\
{														\
	(X)->read_in_progress = -1;										\
	SHM_WRITE_MEMORY_BARRIER;										\
	SET_LATCH((sm_int_ptr_t)&((X)->interlock.latch), LATCH_CLEAR);						\
	SET_LATCH_GLOBAL(&((X)->rip_latch), LOCK_AVAILABLE);							\
}
/* On HPPA, SET_LATCH_GLOBAL forces its available value, for others it is the same as SET_LATCH */

/* New buffer doesn't need interlocked operation.  */
#define LOCK_NEW_BUFF_FOR_UPDATE(X)	(SET_LATCH((sm_int_ptr_t)&((X)->interlock.latch), LATCH_SET))

/* After this macro returns, Y points to the PRE-aswp (not POST-aswp) value of the latch */
#define LOCK_BUFF_FOR_UPDATE(X,Y,Z)     {Y = ASWP((sm_int_ptr_t)&(X)->interlock.latch, LATCH_SET, Z);		\
						 LOCK_HIST("OBTN", &(X)->interlock.latch, process_id, -1);}
#define RELEASE_BUFF_UPDATE_LOCK(X,Y,Z) {LOCK_HIST("RLSE", &(X)->interlock.latch, process_id, -1); 		\
					         Y = ASWP((sm_int_ptr_t)&(X)->interlock.latch, LATCH_CLEAR, Z);}
#define CLEAR_BUFF_UPDATE_LOCK(X,Z)     (ASWP((sm_int_ptr_t)&(X)->interlock.latch, LATCH_CLEAR, Z))

/* Only the writer uses the LATCH_CONFLICT value.  */
#define LOCK_BUFF_FOR_WRITE(X,Y,Z)      (Y = ASWP((sm_int_ptr_t)&(X)->interlock.latch, LATCH_CONFLICT, Z))

/* The macros below currently use an interlocked increment/decrement type of locking unlike the above macros which
 * use an atomic swap method.
 */
#define LOCK_BUFF_FOR_READ(X,Y)											\
{														\
	Y = INCR_CNT((sm_int_ptr_t)&(X)->read_in_progress, (sm_global_latch_ptr_t)&(X)->rip_latch);		\
	READ_LOCK_READ_MEMBARRIER;										\
}

#define RELEASE_BUFF_READ_LOCK(X)										\
{														\
	READ_LOCK_WRITE_MEMBARRIER;										\
	DECR_CNT((sm_int_ptr_t)&(X)->read_in_progress, (sm_global_latch_ptr_t)&(X)->rip_latch);			\
}

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

#define INCR_CNT(X,Y)			INTERLOCK_ADD(X, 1)
#define DECR_CNT(X,Y)			INTERLOCK_ADD(X, -1)

/* Doing the simple test before COMPSWAP_LOCK can help performance when a lock is highly contended */
#define GET_SWAPLOCK(X)		(((X)->u.parts.latch_pid == LOCK_AVAILABLE) && COMPSWAP_LOCK((X), LOCK_AVAILABLE, process_id))

/* Use COMPSWAP_UNLOCK to release the lock because of the memory barrier and other-processor notification it implies. Also
 * the usage of COMPSWAP_UNLOCK allows us to check (with low cost) that we have/had the lock we are trying to release.
 */
#define RELEASE_SWAPLOCK(X)		COMPSWAP_UNLOCK((X), process_id, LOCK_AVAILABLE)

#define	GRAB_LATCH_INDEFINITE_WAIT	-1	/* special value indicating infinite timeout input to "grab_latch" */

#ifdef __x86_64__
#	define	SPINLOCK_PAUSE	_mm_pause()	/* This intrinsic expands to the PAUSE instruction on x86_64 which provides a
						 * hint to the processor that is beneficial in hardspin/spinlock loops. It can
						 * a) Reduce power consumption and
						 * b) More importantly improve spin loop (like this for loop) performance
						 *    by preventing memory-order mis-speculation and avoiding pipeline flushes.
						 */
#elif defined(__aarch64__)
#	define	SPINLOCK_PAUSE	__asm__ __volatile__ ("yield")	/* This is needed because __yield() ACLE intrinsic does not work.
								 * See https://stackoverflow.com/a/70076751 for more details.
								 */
#else
#	error UNSUPPORTED PLATFORM
#endif

