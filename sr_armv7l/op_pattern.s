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

/* op_pattern.s */


	.title	op_pattern.s

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.sbttl	op_pattern

	.text
	.extern	do_patfixed
	.extern	do_pattern

ENTRY op_pattern
	push	{r4, lr}
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	r4, [r1, #mval_a_straddr]
	/*
	 * This is an array of unaligned ints. If the first word is zero, then
	 * call do_pattern instead of do_patfixed. Only the low order byte is
	 * significant and so it is the only one we need to test. We would do
	 * this in assembly because (1) we need the assmembly routine anyway
	 * to save the return value into $TEST and (2) it saves an extra level
	 * of call linkage at the C level to do the decision here.
	 */
	ldrb	r4, [r4]
	cmp	r4, #0
	beq	l1
	bl	do_patfixed
	b	l2

l1:	bl	do_pattern
l2:	cmp	r0, #0
	pop	{r4, pc}

	.end
