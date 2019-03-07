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

/* op_contain.s */

/*
	OP_CONTAIN implements the MUMPS string relational operator
	"[" ("contains"):

		lhs [ rhs

	lhs ("left-hand-side") and rhs ("right-hand-side") are expressions
	interpreted as strings.  If rhs is contained exactly somewhere in lhs,
	the resulting relation is true, otherwise, false.

	On entry to this routine:
		x0 -> mval for lhs
		x1 -> mval for rhs
*/

	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.data
	.extern	matchc
	.extern	n2s
	.extern underr

arg5		=  0				/* Pointer to result */
arg6		=  8				/* Pointer to "numpcs" count */
SAVE_SIZE	= 16

	.text
	
ENTRY op_contain
	stp	x29, x30, [sp, #-16]!
	CHKSTKALIGN				/* Verify stack alignment */
	mov	x29, sp
	sub	sp, sp, SAVE_SIZE
	mov	x27, x1
	mv_force_defined x0
	mov	x26, x0
	mv_force_str x0
	mov	x1, x27
	mv_force_defined x1
	mov	x27, x1
	mv_force_str x1

	add	x5, sp, #arg6			/* Now x5 has address on the stack */
	mov	w15, #1
	str	w15, [x5]			/* And init arg6 to pointer to 1 */
	add	x4, sp, #arg5			/* Pointer to result (arg5) */
	ldr	x3, [x26, #mval_a_straddr]	/* pointer to source string */
	ldr	w2, [x26, #mval_l_strlen]	/* source string length */
	ldr	x1, [x27, #mval_a_straddr]	/* pointer to delimiter string */
	ldr	w0, [x27, #mval_l_strlen]	/* delimiter string length */

	bl	matchc
	ldr	w15, [sp, #arg5]		/* Result after matchc */
	cmp	w15, wzr			/* z-flag set says there was no "match" */
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
