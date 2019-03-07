#################################################################
#								#
# Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	#
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
		x0 -> lhs mval
		x1 -> rhs mval

	Exit:
		x0 =	>0, if lhs follows rhs
			 0, if lhs equals rhs
			<0, if rhs follows lhs (allows reversal of operands and
			    subsequent test)
*/


	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.text
	.extern	memvcmp
	.extern	n2s

ENTRY op_follow
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN				/* Verify stack alignment */
	mov	x28, x1
	mv_force_defined x0
	mov	x27, x0
	mv_force_str x0
	mov	x1, x28
	mv_force_defined x1
	mov	x28, x1
	mv_force_str x1

	ldr	x3, [x28, #mval_l_strlen]
	ldr	x2, [x28, #mval_a_straddr]
	ldr	x1, [x27, #mval_l_strlen]
	ldr	x0, [x27, #mval_a_straddr]
	bl	memvcmp
	cmp	w0, wzr
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
