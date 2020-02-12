#################################################################
#								#
# Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	#
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

/* op_bxrelop.s */


	.include "linkage.si"
#	include "debug.si"

# args:
#	See bxrelop_operator.c for input args details
#
	.text
	.extern	bxrelop_operator

ENTRY op_bxrelop
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN				/* Verify stack alignment */
	bl	bxrelop_operator
	cmp	w0, wzr
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
