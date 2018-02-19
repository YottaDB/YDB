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

/* opp_svput.s */

/*
 * void op_svput(int varnum, mval *v)
 */

	.title	opp_svput.s

	.include "linkage.si"
	.include "g_msf.si"
	.include "stack.si"
#	include "debug.si"

	.sbttl	opp_svput

	.data
	.extern	frame_pointer

	.text
	.extern	op_svput

ENTRY opp_svput
	mov	fp, sp					/* Save sp against potention adjustment */
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_svput
	getframe
	mov	sp, fp					/* Restore sp */
	bx	lr

	.end
