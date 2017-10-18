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
		r0	mval *u
		r1	mval *v

	exit:
		condition codes set according to value of
			numcmp (u, v)
*/

	.title	op_numcmp.s

	.sbttl	op_numcmp

	.include "linkage.si"
#	include "debug.si"

	.text
	.extern	numcmp

ENTRY op_numcmp
	push	{r4, lr}				/* r4 is to maintain 8 byte stack alignment */
	CHKSTKALIGN					/* Verify stack alignment */
	bl	numcmp
	cmp	r0, #0					/* set flags according to result from numcmp */
	pop	{r4, pc}

	.end
