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

/* op_restartpc.s */

	.title	op_restartpc.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	op_restartpc

	.data
	.extern	restart_pc
	.extern restart_ctxt
	.extern frame_pointer

	.text

/*
 * Routine to save the address of the *start* of this call along with its context as the restart point should this
 * process encounter a restart situation (return from $ZTRAP or outofband call typically).
 *
 * Since this is a leaf routine (makes no calls), the stack frame alignment is not important so is not adjusted
 * or tested. Should that change, the alignment should be fixed and implement use of the CHKSTKALIGN macro made.
 */
ENTRY op_restartpc
	sub	r4, lr, #8			/* xfer call size is constant, 4 for ldr and 4 for blx */
	ldr	r1, =restart_pc
	str	r4, [r1]
	ldr	r12, [r5]
	ldr	r4, [r12, #msf_ctxt_off]
	ldr	r2, =restart_ctxt
	str	r4, [r2]
	bx	lr

	.end
