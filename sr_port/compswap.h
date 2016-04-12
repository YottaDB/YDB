/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef COMPSWAP_H_INCLUDE
#define COMPSWAP_H_INCLUDE

/* COMPSWAP_LOCK/UNLOCK are the same for all platform except for __ia64, which needs slightly different versions to handle
 * memory consistency isues
 */
#ifdef UNIX
	boolean_t compswap_secshr(sm_global_latch_ptr_t lock, int compval, int newval1);
#	if (defined(_AIX) || (defined(__ia64) && defined(__linux__)))	/* AIX or Linux Itanium */
		boolean_t compswap_lock(sm_global_latch_ptr_t lock, int compval, int newval1);
		boolean_t compswap_unlock(sm_global_latch_ptr_t lock, int compval, int newval1);
#		define COMPSWAP_LOCK(LCK, CMPVAL1, CMPVAL2, NEWVAL1, NEWVAL2)	compswap_lock(LCK, CMPVAL1, NEWVAL1)
#		define COMPSWAP_UNLOCK(LCK, CMPVAL1, CMPVAL2, NEWVAL1, NEWVAL2)	compswap_unlock(LCK, CMPVAL1, NEWVAL1)
#	elif !defined(__ia64)
		boolean_t compswap(sm_global_latch_ptr_t lock, int compval, int newval1);
#		define COMPSWAP_LOCK(LCK, CMPVAL1, CMPVAL2, NEWVAL1, NEWVAL2)	compswap(LCK, CMPVAL1, NEWVAL1)
#		define COMPSWAP_UNLOCK(LCK, CMPVAL1, CMPVAL2, NEWVAL1, NEWVAL2)	compswap(LCK, CMPVAL1, NEWVAL1)
#	elif (defined(__HP_cc) || (defined(__hpux) && defined(__GNUC__)))
		/* Use compiler inline assembly macros for HP-UX/HP C or GCC on HPUX*/
		/* This is assuming 32 bit lock storage, which right now seems to be PIDs
		 * most of the time. PIDs are currently 32 bit values, but that could change
		 * someday, so beware
		 */
#		include <ia64/sys/inline.h>
#		define FENCE	(_Asm_fence) (_UP_CALL_FENCE | _UP_SYS_FENCE | _DOWN_CALL_FENCE | _DOWN_SYS_FENCE)
#		define COMPSWAP_LOCK(LCK, CMPVAL1, CMPVAL2, NEWVAL1, NEWVAL2)					\
		(												\
			_Asm_mov_to_ar((_Asm_app_reg)_AREG_CCV, (uint64_t) CMPVAL1,FENCE),			\
			_Asm_cmpxchg((_Asm_sz)_SZ_W, (_Asm_sem)_SEM_ACQ,(uint32_t *)LCK,			\
				(uint64_t)NEWVAL1, (_Asm_ldhint)_LDHINT_NONE) == (uint64_t)CMPVAL1 ? 1 : 0	\
		)
#		define COMPSWAP_UNLOCK(LCK, CMPVAL1, CMPVAL2, NEWVAL1, NEWVAL2)					\
		(												\
			_Asm_mov_to_ar((_Asm_app_reg)_AREG_CCV,(uint64_t) CMPVAL1,FENCE),			\
			_Asm_cmpxchg((_Asm_sz)_SZ_W,(_Asm_sem)_SEM_REL,(uint32_t *)LCK,				\
				(uint64_t)NEWVAL1, (_Asm_ldhint)_LDHINT_NONE) == (uint64_t)CMPVAL1 ? 1 : 0	\
		)
#	else
#		error Unsupported Platform sr_port/compswap.h
#	endif /* __ia64 */
#else
	boolean_t compswap(sm_global_latch_ptr_t lock, int compval1, int compval2, int newval1, int newval2);
	boolean_t compswap_secshr(sm_global_latch_ptr_t lock, int compval1, int compval2, int newval1, int newval2);
#	define COMPSWAP_LOCK(LCK, CMPVAL1, CMPVAL2, NEWVAL1, NEWVAL2)		compswap(LCK, CMPVAL1, CMPVAL2, NEWVAL1, NEWVAL2)
#	define COMPSWAP_UNLOCK(LCK, CMPVAL1, CMPVAL2, NEWVAL1, NEWVAL2)		compswap(LCK, CMPVAL1, CMPVAL2, NEWVAL1, NEWVAL2)
#endif

#endif
