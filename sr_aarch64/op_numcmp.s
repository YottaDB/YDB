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

/* op_numcmp.s */
/*
	op_numcmp calls numcmp to compare two mvals

	entry:
		x0	mval *u
		x1	mval *v

	exit:
		condition codes set according to value of
			numcmp (u, v)
*/

	.include "linkage.si"
#	include "debug.si"

	.text
	.extern	numcmp


ENTRY op_numcmp
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN					/* Verify stack alignment */
	bl	numcmp
	cmp	x0, xzr					/* set flags according to result from numcmp */
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
