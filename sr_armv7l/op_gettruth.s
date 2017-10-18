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

/* op_gettruth.s */

	/* r1 - mval where $T value is copied */

	.title	op_gettruth.s

	.include "linkage.si"
	.include "mval_def.si"

	.sbttl	op_gettruth

	.data
	.extern	dollar_truth
	.extern	literal_one
	.extern	literal_zero

	.text
/*
 * Routine to fetch mval representing value of $TEST (formerly $TRUTH).
 *
 * Note this routine is a leaf routine so does no stack-alignment or checking. If that changes, this routine
 * needs to use CHKSTKALIGN macro and make sure stack is 8 byte aligned.
 */
ENTRY op_gettruth
	ldr	r3, =dollar_truth
	ldr	r0, [r3]
	cmp	r0, #0
	bne	l1
	ldr	r2, =literal_zero
	b	l2

l1:	ldr	r2, =literal_one
l2:	mov	r3, #mval_byte_len
l3:	ldr	r0, [r2], #+4
	str	r0, [r1], #+4
	subs	r3, #4
	bgt	l3
	bx	lr

	.end
