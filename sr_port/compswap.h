/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017-2020 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 * Copyright (c) 2018 Stephen L Johnson.			*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef COMPSWAP_H_INCLUDE
#define COMPSWAP_H_INCLUDE

/* COMPSWAP_LOCK/UNLOCK are the same for some platforms and different for some platforms. */

# if defined(__armv6l__) || defined(__armv7l__) || defined(__aarch64__)
	/* Linux on ARMV6L and ARMV7L and AARCH64 */
	boolean_t compswap_lock(sm_global_latch_ptr_t lock, int compval, int newval);
	boolean_t compswap_unlock(sm_global_latch_ptr_t lock);
#	define COMPSWAP_LOCK(LCK, CMPVAL, NEWVAL)	compswap_lock(LCK, CMPVAL, NEWVAL)
#	define COMPSWAP_UNLOCK(LCK, CMPVAL, NEWVAL)	(DBG_ASSERT((LCK)->u.parts.latch_pid == CMPVAL) DBG_ASSERT(NEWVAL == 0)	\
							compswap_unlock(LCK))

# else
	/* Linux on x86_64 (for now) */
	boolean_t compswap(sm_global_latch_ptr_t lock, int compval, int newval1);
#	define COMPSWAP_LOCK(LCK, CMPVAL, NEWVAL)	compswap(LCK, CMPVAL, NEWVAL)
	/* Note that most of the time CMPVAL would be the same as (LCK)->u.parts.latch_pid in case of a COMPSWAP_UNLOCK call
	 * since the lock holder will invoke this macro and knows exactly what the CMPVAL is. But it is also possible the
	 * COMPSWAP_UNLOCK macro is called by a pid that is not the lock holder (e.g. jnl_write_attempt(), prepare_for_gc() etc.)
	 * but is trying to salvage the lock in case of a dead process. In that case, the CMPVAL value that the salvaging
	 * process passes in could not be the actual value of the "latch_pid" because there could be multiple processes trying
	 * the salvage at the same time. Hence we cannot add "DBG_ASSERT((LCK)->u.parts.latch_pid == CMPVAL)" to the below macro.
	 */
#	define COMPSWAP_UNLOCK(LCK, CMPVAL, NEWVAL)	(DBG_ASSERT(NEWVAL == 0) compswap(LCK, CMPVAL, NEWVAL))

# endif

#endif
