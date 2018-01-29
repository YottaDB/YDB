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

/* aswp.s */
/* Atomic swap operation
 *	int aswp(sm_int_ptr_t *loc, int value)
 *	atomically set *loc to value and return the old value of *loc
 *
 *		r0 - pointer to loc
 *		r1 - value
*/

	.title	aswp.s
	.sbttl	aswp

	.include "linkage.si"

	.text

RETRY_COUNT	=	1024

/*
 * Note since this routine makes no calls, stack alignment is not critical. If ever a call is added then this
 * routine should take care to align the stack to 8 bytes and add a CHKSTKALIGN macro.
 */
ENTRY aswp
	mov	r12, #RETRY_COUNT
.ifdef __armv7l__
	dmb
.else	/* __armv6l__ */
	mcr	p15, 0, r0, c7, c10, 5	/* equivalent to armv7l's "dmb" on armv6l */
.endif
retry:
	ldrex	r2, [r0]
	strex	r3, r1, [r0]
	cmp	r3, #1
	beq	store_failed
.ifdef __armv7l__
	dmb
.else	/* __armv6l__ */
	mcr	p15, 0, r0, c7, c10, 5	/* equivalent to armv7l's "dmb" on armv6l */
.endif
	mov	r0, r2
	bx	lr
store_failed:
	subs	r12, #1
	bne	retry
	mov	r12, #RETRY_COUNT
	b	retry
	.end

