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

/* lockconst.h - define values used for interlocks */

#define LOCK_AVAILABLE	 	0
#define LOCK_IN_USE	 	1
#define REC_UNDER_PROG          2
/* CAUTION: the following two values are not currently optional on HPUX,
	    but rather chosen for HP-PA arthitectural reasons */
#define GLOBAL_LOCK_AVAILABLE	-1
#define GLOBAL_LOCK_IN_USE	0

/* Global latch note for platforms with a micro latch (currently Solaris
   and HPUX). The SET_LATCH_GLOBAL macro initializes both the compswap
   lock (major latch) and the micro latch used by compswap. The order this is done in is
   first the micro latch, then the major latch. The order is important so that
   secshr_db_clnup can (re)initialize the entire latch and not run into problems
   with concurrent users attempting to get the lock. The compswap routines in
   these modules will check the value of the major latch first before even attempting
   to obtain the micro latch.
*/

#ifdef __hppa
#  define alignedaddr(x) (volatile int *)((UINTPTR_T)(x) + 15 & ~0xf)  /* 32-bit */

    /* Given a pointer into memory, round up to the nearest 16-byte boundary
       by adding 15, then masking off the last four bits.
       Assumption: the input address is already int (4-byte) aligned.

       The VOLATILE keyword is essential in this macro: it ensures that the
       compiler does not perform certain optimizations which would compromise
       the integrity the spinlock logic.
     */

#  define release_spinlock(lockarea)  {if (1) {                       \
        _flush_globals();                                           \
        (*alignedaddr(&(lockarea)->hp_latch_space) = GLOBAL_LOCK_AVAILABLE); } else ;}

/* HP white paper sets latch to 1 for available while we set it to -1 */

    /* For performance, release_spinlock is a macro, rather than a function.
       To release or initialize a spinlock, we simply set its value to one.

       We must call the psuedo function "_flush_globals()" to ensure that
       the compiler doesn't hold any externally-visible values in registers
       across the lock release */

   int4 load_and_clear(sm_int_ptr_t);
#  define GET_LATCH_GLOBAL(a)     (GLOBAL_LOCK_AVAILABLE == *alignedaddr(&(a)->hp_latch_space) ? \
                                        load_and_clear((sm_int_ptr_t)&(a)->hp_latch_space) : GLOBAL_LOCK_IN_USE)
        /* above tries a fast pretest before calling load_and_clear to actually
           get the latch */
#  define RELEASE_LATCH_GLOBAL(a) release_spinlock(a)
#  define SET_LATCH_GLOBAL(a, b)	{RELEASE_LATCH_GLOBAL(a); assert(LOCK_AVAILABLE == b); SET_LATCH(a, b);}

#elif defined(__sparc) && defined(SPARCV8_NO_CAS)
/* For Sun sparc, we use the extra word of the latch for a micro lock for compswap. Future
   iterations of this should make use of the CAS (compare and swap) instruction newly available
   in the Sparc Version 9 instruction set.
   These *_GLOBAL macros are used only from compswap.c (currently)
*/
#  define GET_LATCH_GLOBAL(a)	aswp(&(a)->u.parts.latch_word, GLOBAL_LOCK_IN_USE)
#  define RELEASE_LATCH_GLOBAL(a) aswp(&(a)->u.parts.latch_word, GLOBAL_LOCK_AVAILABLE)
#  define SET_LATCH_GLOBAL(a, b) {SET_LATCH(&(a)->u.parts.latch_word, GLOBAL_LOCK_AVAILABLE); SET_LATCH(a, b);}

#elif defined(VMS)
#  define SET_LATCH_GLOBAL(a, b) {(a)->u.parts.latch_image_count = 0; SET_LATCH(a, b);}
#else
#  define SET_LATCH_GLOBAL(a, b) SET_LATCH(a, b)
#endif

/* perhaps this should include flush so other CPUs see the change now */
#define SET_LATCH(a,b)		(*((sm_int_ptr_t)a) = b)
