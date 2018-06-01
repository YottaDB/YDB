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

/* mint2mval.s */
/*	Convert to int to mval
 *		x0 - pointer to mval to receive value
 *		w1 - int to convert
 */

	.include "g_msf.si"
	.include "linkage.si"
#	include "debug.si"

	.text

ENTRY mint2mval
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN					/* Verify stack alignment */
	bl	i2mval
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
