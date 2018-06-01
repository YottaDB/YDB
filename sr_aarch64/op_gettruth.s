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

	/* x1 - mval where $T value is copied */

	.include "linkage.si"
	.include "mval_def.si"

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
	ldr	w9, [x25]
	cbnz	w9, l1
	ldr	x11, =literal_zero
	b	l2
l1:
	ldr	x11, =literal_one
l2:
	mov	x12, #mval_byte_len
l3:
	ldr	x9, [x11], #+8
	str	x9, [x1], #+8
	subs	x12, x12, #8
	b.gt	l3
	ret
