/****************************************************************
 *								*
 * Copyright (c) 2024-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef INLINE_UPDATE_IN_CW_SET_INCLUDED
#define INLINE_UPDATE_IN_CW_SET_INCLUDED

#include "mdef.h"
#include "gtm_atomic.h"

DEFINE_ATOMIC_OP(gtm_atomic_uint, ATOMIC_COMPARE_EXCHANGE, memory_order_acq_rel, memory_order_relaxed)
DEFINE_ATOMIC_OP(gtm_atomic_uint, ATOMIC_STORE, memory_order_relaxed)
DEFINE_ATOMIC_OP(gtm_atomic_uint, ATOMIC_LOAD, memory_order_acquire)

/* Atomically do:  if (*pid_addr == expect) *pid_addr = value
 * 	- addr	 is the address of the *pid_addr uint4
 * 	- expect is the expected value
 * 	- value	 is the desired value to set only if expect is actual
 *	returns true if exchange succeeded
 * This function conditionally updates *pid_addr
 */
static inline bool inline_atomic_pid_set_if_exp(uint4 *pid_addr, uint4 expect, uint4 value)
{
	return ATOMIC_COMPARE_EXCHANGE_STRONG((gtm_atomic_uint *)pid_addr, &expect, value,
						memory_order_acq_rel, memory_order_relaxed);
}

/* Atomically do:  *pid_addr = value
 * 	- addr	 is the address of the *pid_addr uint4
 * 	- value	 is the desired value to set
 * This function unconditionally updates *pid_addr
 */
static inline void inline_atomic_pid_set(uint4 *pid_addr, uint4 value)
{
	ATOMIC_STORE((gtm_atomic_uint *)pid_addr, value, memory_order_relaxed);
}

/* Atomically get *pid_addr
 * 	- addr	 is the address of the *pid_addr uint4
 * 	returns the value of *pid_addr
 * This function unconditionally reads *pid_addr with a memory barrier
 */
static inline uint4 inline_atomic_pid_get(uint4 *pid_addr)
{
	return ATOMIC_LOAD((gtm_atomic_uint *)pid_addr, memory_order_acquire);
}

#endif /* INLINE_UPDATE_IN_CW_SET_INCLUDED */
