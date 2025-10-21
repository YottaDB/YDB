#################################################################
#								#
# Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2018 Stephen L Johnson. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

/* compswap.s */

/*    boolean_t compswap_lock(sm_global_latch_ptr_t latch, int compval, int newval)
 *	If the supplied latch matches the comparison value, the new value
 *	is stored in the latch atomically.
 *     Return TRUE if swap/store succeeds,otherwise return FALSE.
 *
 *		x0 - Pointer to latch
 *		w1 - comparison value
 *		w2 - replacement value
 */

	.include "linkage.si"

	.text

/* Note since this routine makes no calls, stack alignment is not critical. If ever a call is added then this
 * routine should take care to align the stack to 8 bytes and add a CHKSTKALIGN macro.
 */

ENTRY compswap_lock
trylock_loop:
	ldxr	w3, [x0]		/* get latch value */
	cmp	w3, w1
	b.ne	nomatch
	stlxr	w3, w2, [x0]		/* only do swap if latch value matches comparison value */
	cbnz	w3, trylock_loop
	dmb	ish			/* ensures that all subsequent accesses are observed (by a concurrent process)
					 * AFTER the gaining of the lock is observed.
					 */
	mov	x0, #1			/* return success */
	cmp	x0, xzr
	ret
nomatch:
	clrex				/* reset exclusive status */
	mov	x0, xzr			/* return failure */
	cmp	x0, xzr
	ret

/*    boolean_t compswap_unlock(sm_global_latch_ptr_t latch)
 *		x0 - Pointer to latch
 *	Stores 0 in *x0 and returns
 */
ENTRY compswap_unlock
	mov	w1, wzr
	dmb	ish			/* ensures that all previous accesses are observed (by a concurrent process)
					 * BEFORE the clearing of the lock is observed.
					 */
	str	w1, [x0]
	ret

.end
