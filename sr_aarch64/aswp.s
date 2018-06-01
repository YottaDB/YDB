#################################################################
#								#
# Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2017 Stephen L Johnson. All rights reserved.	#
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

RETRY_COUNT	=	1024

/*
 * Note since this routine makes no calls, stack alignment is not critical. If ever a call is added then this
 * routine should take care to align the stack to 16 bytes and add a CHKSTKALIGN macro.
 */
ENTRY aswp
	mov	w13, #RETRY_COUNT
	dmb	ish			/* xxxxxxx is ish the proper option */
retry:
	ldxr	w10, [x0]
	stlxr	w11, w1, [x0]
	cmp	w11, #1
	b.eq	store_failed
	dmb	ish			/* xxxxxxx is ish the proper option */
	mov	w0, w10
	ret
store_failed:
	subs	w10, w10, #1
	bne	retry
	mov	w10, #RETRY_COUNT
	b	retry

