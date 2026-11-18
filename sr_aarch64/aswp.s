#################################################################
#								#
# Copyright (c) 2018-2026 YottaDB LLC and/or its subsidiaries.	#
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

/* aswp.s */
/* Atomic swap operation
 *	int aswp(sm_int_ptr_t *loc, int value)
 *	atomically set *loc to value and return the old value of *loc
 *
 *		x0 - pointer to loc
 *		w1 - value
*/

	.include "linkage.si"

	.text

/* The swap is ordered with respect to all memory accesses before and after it, without a "dmb ish" before the loop:
 * - The store-release (stlxr) makes every earlier access visible before the new value is stored.
 * - The load-exclusive (ldxr) can read the old value before the earlier accesses are visible, but the store-exclusive
 *   succeeds only if no other processor wrote the word since that load. So when the loop ends, the old value read is
 *   the value that the store replaced, and the swap takes effect as a whole at the store-release, after the earlier
 *   accesses.
 * - The "dmb ish" after the loop keeps every later access from being performed before the swap.
 * This is the sequence that the Linux kernel uses on arm64 for its fully ordered xchg().
 * Note since this routine makes no calls, stack alignment is not critical. If ever a call is added then this
 * routine should take care to align the stack to 16 bytes and add a CHKSTKALIGN macro.
 */
ENTRY aswp
retry:
	ldxr	w10, [x0]
	stlxr	w11, w1, [x0]
	cbnz	w11, retry			/* the store failed because another access intervened; try again */
	dmb	ish
	mov	w0, w10
	ret
