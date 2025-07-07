/****************************************************************
 *								*
 * Copyright (c) 2025 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef INLINE_NOT_FROZEN_INCLUDED
#define INLINE_NOT_FROZEN_INCLUDED

#include "gtm_atomic.h"

/* Atomically get *freeze_addr
 * 	- addr	 is the address of the *freeze_addr uint4
 * 	returns the value of *freeze_addr
 * This function unconditionally reads *freeze_addr with a memory barrier
 */
static inline uint4 inline_atomic_freeze_get(uint4 *freeze_addr)
{
	return ATOMIC_LOAD((gtm_atomic_uint *)freeze_addr, memory_order_acquire);
}

/* Return TRUE if the database is not hard frozen */
static inline bool not_frozen_hard(sgmnt_addrs *csa)
{
	uint4	lcl_freeze, lcl_freeze_online, lcl_latch_pid;

	lcl_latch_pid = inline_atomic_freeze_get((uint4 *)&csa->nl->freeze_latch.u.parts.latch_pid);
	lcl_freeze = inline_atomic_freeze_get((uint4 *)&csa->hdr->freeze);
	lcl_freeze_online = inline_atomic_freeze_get((uint4 *)&csa->nl->freeze_online);

	return (!(lcl_freeze && !lcl_freeze_online) || (0 != lcl_latch_pid));
}

/* Return TRUE if the database is not chilled */
static inline bool not_frozen_chilled(sgmnt_addrs *csa)
{
	uint4	lcl_freeze, lcl_freeze_online, lcl_latch_pid;

	lcl_latch_pid = inline_atomic_freeze_get((uint4 *)&csa->nl->freeze_latch.u.parts.latch_pid);
	lcl_freeze = inline_atomic_freeze_get(&csa->hdr->freeze);
	lcl_freeze_online = inline_atomic_freeze_get(&csa->nl->freeze_online);

	return (!(lcl_freeze && lcl_freeze_online) || (0 != lcl_latch_pid));
}

#endif /* INLINE_NOT_FROZEN_INCLUDED */
