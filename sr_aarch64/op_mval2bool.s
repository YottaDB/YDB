#################################################################
#								#
# Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	#
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
#
# op_mval2bool.s
#	Convert mval to bool.
# args:
#	See mval2bool.c for input args details
#
	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.text
	.extern	mval2bool

ENTRY op_mval2bool
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN		/* Verify stack alignment */
	bl	mval2bool	/* Call C function `mval2bool` with the same parameters that we were passed in with.
				 * This does the bulk of the needed $ZYSQLNULL processing for boolean expression evaluation.
				 * The `bool_result` return value would be placed in `x0`.
				 */
	cmp	x0, xzr
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
