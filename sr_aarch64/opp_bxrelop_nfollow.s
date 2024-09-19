#################################################################
#								#
# Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	#
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

/* opp_bxrelop_nfollow.s */


	.include "linkage.si"
#	include "debug.si"

	.text
	.extern	op_bxrelop_nfollow

ENTRY opp_bxrelop_nfollow
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN				/* Verify stack alignment */
	bl	op_bxrelop_nfollow
	cmp	w0, wzr
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
