#################################################################
#								#
# Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

/* opp_nequ_retbool.s */


	.include "linkage.si"
#	include "debug.si"

# args:
#	See op_nequ_retbool.c for input args details
#
	.text
	.extern	op_nequ_retbool

ENTRY opp_nequ_retbool
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN				/* Verify stack alignment */
	bl	op_nequ_retbool
				/* Call C function `op_nequ_retbool` with the same parameters that we were passed in with.
				 * That does the bulk of the needed processing.
				 * The `ret` boolean return value would be placed in 64-bit "x0" (i.e. 32-bit "w0").
				 */
	cmp	w0, wzr
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
