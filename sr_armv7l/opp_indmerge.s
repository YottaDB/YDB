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

/* opp_indmerge.s */

/*
 * void op_indmerge(mval *glvn_mv, mval *arg1_or_arg2)
 */

	.title	opp_indmerge.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	opp_indmerge

	.data
	.extern	frame_pointer

	.text
	.extern	op_indmerge

ENTRY opp_indmerge
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indmerge
	getframe
	bx	lr

	.end
