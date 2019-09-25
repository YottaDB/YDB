#################################################################
#								#
# Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	#
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
	.extern frame_pointer				// Note this indirectly used as its addr is in x19

	.text

/*
 * Routine to save the address of the *start* of this call along with its context as the restart point should this
 * process encounter a restart situation (return from $ZTRAP or outofband call typically).
 *
 * Since this is a leaf routine (makes no calls), the stack frame alignment is not important so is not adjusted
 * or tested. Should that change, the alignment should be fixed and implement use of the CHKSTKALIGN macro made.
 */

ENTRY op_restartpc
	sub	x13, x30, #8				// Xfer call size is constant, 4 for ldr and 4 for blr
	ldr	x10, [x19]				// Load contents of frame_pointer
	str	x13, [x10, #msf_restart_pc_off]		// Save restart pc in stack frame
	ldr	x12, [x10, #msf_ctxt_off]		// Fetch frame's resume context
	str	x12, [x10, #msf_restart_ctxt_off]	// Save as the restart context in this frame as well
	ret
