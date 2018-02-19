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

/* follow.s */

	.title	follow.s
	.sbttl	follow

	.include "linkage.si"
	.include "stack.si"
#	include "debug.si"

	.text
	.extern	op_follow

ENTRY follow
	push	{fp, lr}
	mov	fp, sp
	CHKSTKALIGN				/* Verify stack alignment */
	bl	op_follow
	ble	notfollow
	movs	r0, #1
	b	done
notfollow:
	movs	r0, #0
done:
	mov	sp, fp
	pop	{fp, pc}

	.end
