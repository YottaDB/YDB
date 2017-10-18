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

/* op_follow.s */
/*
	op_follow implements the string relational operator "]" ("follows"):

		lhs ] rhs

	lhs ("left-hand-side") and rhs ("right-hand-side") are expressions
	interpreted as strings.  If lhs follows rhs in the ASCII collating
	sequence, the resulting	relation is true, otherwise, false (actually,
	this function differs slightly -- see description of exit conditions
	below).

	According to the ANSI standard, ANSI/MDC X11.1-1990, the relation is
	true iff any of the following is true:

		a.  rhs is empty and lhs is not.
		b.  neither lhs nor rhs is empty and the leftmost character of
		    lhs follows (has numeric code greater than) the leftmost
		    character of rhs.
		c.  There exists a positive integer n such that lhs and rhs have
		    identical heads of length n (i.e., $E(lhs,1,n)=$E(rhs,1,n))
		    and the remainder of lhs follows the remainder of rhs.

	Entry:
		r0 -> lhs mval
		r1 -> rhs mval

	Exit:
		r0 =	>0, if lhs follows rhs
			 0, if lhs equals rhs
			<0, if rhs follows lhs (allows reversal of operands and
			    subsequent test)
*/

	.title	op_follow.s

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.sbttl	op_follow

sav_r0	=	-4
sav_r1	=	-8

	.text
	.extern	memvcmp
	.extern	n2s
	.extern underr

ENTRY op_follow
	push	{r4, r6, fp, lr}
	CHKSTKALIGN				/* Verify stack alignment */
	mov	fp, sp
	sub	sp, #8

	str	r1, [fp, #sav_r1]
	mv_force_defined r0
	str	r0, [fp, #sav_r0]
	mv_force_str r0
	ldr	r1, [fp, #sav_r1]
	mv_force_defined r1
	str	r1, [fp, #sav_r1]
	mv_force_str r1

	ldr	r0, [fp, #sav_r0]
	ldr	r1, [fp, #sav_r1]

	ldr	r3, [r1, #mval_l_strlen]
	ldr	r2, [r1, #mval_a_straddr]
	ldr	r1, [r0, #mval_l_strlen]
	ldr	r0, [r0, #mval_a_straddr]
	bl	memvcmp
	cmp	r0, #0

	mov	sp, fp
	pop	{r4, r6, fp, pc}

	.end
