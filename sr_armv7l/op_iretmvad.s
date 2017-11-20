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

/* op_iretmvad.s */

	.title	op_iretmvad.s


	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	op_iretmvad

	.data
	.extern	frame_pointer

	.text
	.extern	op_unwind

	.global	op_iretmvad
	.func	op_iretmvad

ENTRY op_iretmvad
	putframe
	CHKSTKALIGN				/* Verify stack alignment */
	mov	r4, r1				/* Save input mval * value across call to op_unwind */
	bl	op_unwind
	mov	r0, r4				/* Return input mval* */
	getframe				/* Pick up new stack frame regs and return address */
	bx	lr

	.end
