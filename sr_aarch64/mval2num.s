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

/* mval2num.s */
/*	Convert mval to numeric */
/*		on entry: x1 - pointer to mval to convert	*/

	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.text
	.extern	n2s
	.extern	s2n
	.extern underr

ENTRY mval2num
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN				/* Verify stack alignment */
	mv_force_defined x1
	mov	x27, x1				/* Save mval ptr - mv_force_defined may have re-defined it */
	mv_force_num x1
	mov	x1, x27				/* Restore saved mval pointer */
	mv_force_str_if_num_approx x1
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
