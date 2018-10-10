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

/* op_restartpc.s */

	.include "linkage.si"
	.include "g_msf.si"

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
	sub	x13, x30, #8			/* xfer call size is constant, 4 for ldr and 4 for blr */
	ldr	x10, =restart_pc
	str	x13, [x10]
	ldr	x12, [x19]
	ldr	x13, [x12, #msf_ctxt_off]
	ldr	x11, =restart_ctxt
	str	x13, [x11]
	ret
