#################################################################
#								#
# Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2017-2018 Stephen L Johnson.			#
# All rights reserved.						#
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
 *		r0 - Pointer to latch
 *		r1 - comparison value
 *		r2 - replacement value
 */

	.title	compswap.s
	.sbttl	compswap

	.include "linkage.si"

	.text

/* Note since this routine makes no calls, stack alignment is not critical. If ever a call is added then this
 * routine should take care to align the stack to 8 bytes and add a CHKSTKALIGN macro.
 */

ENTRY compswap_lock
	ldrex	r3, [r0]		/* get latch value */
	cmp	r3, r1
	bne	nomatch
	strexeq	r3, r2, [r0]		/* only do swap if latch value matches comparison value */
	cmpeq	r3, #1
	beq	notset			/* store-exclusive failed */
.ifdef __armv7l__
	dmb				/* ensures that all subsequent accesses are observed (by a concurrent process)
					 * AFTER the gaining of the lock is observed.
					 */
.else	/* __armv6l__ */
	mcr	p15, 0, r0, c7, c10, 5	/* equivalent to armv7l's "dmb" on armv6l */
.endif
	movs	r0, #1			/* return success */
	bx	lr
nomatch:
	clrex				/* reset exclusive status */
notset:
	movs	r0, #0			/* return failure */
	bx	lr

/*    boolean_t compswap_unlock(sm_global_latch_ptr_t latch)
 *		r0 - Pointer to latch
 *	Stores 0 in *r0 and returns
 */
ENTRY compswap_unlock
	mov	r1, #0
.ifdef __armv7l__
	dmb				/* ensures that all previous accesses are observed (by a concurrent process)
					 * BEFORE the clearing of the lock is observed.
					 */
.else	/* __armv6l__ */
	mcr	p15, 0, r0, c7, c10, 5	/* equivalent to armv7l's "dmb" on armv6l */
.endif
	str	r1, [r0]
	bx	lr

	.end
