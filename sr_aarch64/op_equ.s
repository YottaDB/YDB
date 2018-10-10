#################################################################
#								#
# Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	#
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

/* op_equ.s */

/*
	op_equ

	op_equ calls is_equ to compare two mval operands to determine
	whether they are equal.  The actual comparison is performed by
	the C-callable routine is_equ; op_equ is needed as an interlude
	between generated GT.M code that passes the arguments in r0
	and r0 instead of in the argument registers.

	entry
		x0, x1	contain addresses of mval's to be compared

	return
		x0	1, if the two mval's are equal
			0, if they're not equal
*/

	.include "g_msf.si"
	.include "linkage.si"
#	include "debug.si"

	.text
	.extern	is_equ


ENTRY op_equ
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN				/* Verify stack alignment */
	bl	is_equ
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	cmp	w0, wzr
	ret
