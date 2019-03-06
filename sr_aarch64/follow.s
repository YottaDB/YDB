#################################################################
#								#
# Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	#
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

/* follow.s */


	.include "linkage.si"
#	include "debug.si"

	.text
	.extern	op_follow

ENTRY follow
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN				/* Verify stack alignment */
	bl	op_follow
	b.le	notfollow
	mov	x0, #1
	b	done
notfollow:
	mov	x0, xzr
done:
	cmp	x0, xzr
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
