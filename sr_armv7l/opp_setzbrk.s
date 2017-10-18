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

/* opp_setzbrk.s */

/*
 * void	op_setzbrk(mval *rtn, mval *lab, int offset, mval *act, int cnt)
 *	r0	- *rtn
 *	r1	- *lab
 *	r2	- offset
 *	r3	- act == action associated with ZBREAK
 *	stack	- cnt == perform break after this many passes
 */

	.title	opp_setzbrk.s

	.include "linkage.si"
	.include "g_msf.si"
	.include "stack.si"
#	include "debug.si"

	.sbttl	opp_setzbrk

	.data
	.extern	frame_pointer

	.text
	.extern	op_setzbrk

ENTRY opp_setzbrk
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_setzbrk
	getframe
	bx	lr

	.end
