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

/* op_contain.s */

	.title	op_contain.s "'[' ('contains') operator"

/*
	OP_CONTAIN implements the MUMPS string relational operator
	"[" ("contains"):

		lhs [ rhs

	lhs ("left-hand-side") and rhs ("right-hand-side") are expressions
	interpreted as strings.  If rhs is contained exactly somewhere in lhs,
	the resulting relation is true, otherwise, false.

	On entry to this routine:
		r0 -> mval for lhs
		r1 -> mval for rhs
*/

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.sbttl	op_contain

	.data
	.extern	matchc
	.extern	n2s
	.extern underr

	.text
sav_r0	=	-4
sav_r1	=	-8

ENTRY op_contain
	stmfd	sp!, {fp, lr}
	CHKSTKALIGN				/* Verify stack alignment */
	mov	fp, sp
	sub	sp, sp, #8
	str	r1, [fp, #sav_r1]
	mv_force_defined r0
	str	r0, [fp, #sav_r0]
	mv_force_str r0
	ldr	r1, [fp, #sav_r1]
	mv_force_defined r1
	str	r1, [fp, #sav_r1]
	mv_force_str r1
	mov	r2, #1				/* pieces argument, but have to pass its addr */
	push	{r2}
	mov	r1, sp
	sub	sp, #4				/* returned value */
	mov	r0, sp
	push	{r1}				/* pointer to the number of pieces that are desired to be matched */
	push	{r0}				/* pointer to result */
	ldr	r0, [fp, #sav_r0]
	ldr	r4, [fp, #sav_r1]
	ldr	r3, [r0, #mval_a_straddr]	/* pointer to source string */
	ldr	r2, [r0, #mval_l_strlen]	/* source string length */
	ldr	r1, [r4, #mval_a_straddr]	/* pointer to delimiter string */
	ldr	r0, [r4, #mval_l_strlen]	/* delimiter length */
	bl	matchc
	add	sp, #8				/* skip arguments */
	pop	{r1}				/* return value */
	pop	{r0}				/* updated pieces value (ignored) */
	cmp	r1, #0
	mov	sp, fp
	ldmfd   sp!, {fp, pc}

	.end
