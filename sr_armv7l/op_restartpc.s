#################################################################
#								#
# Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries.	#
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
	.extern frame_pointer				// Note this indirectly used as its addr is in R5

	.text

/*
 * Routine to save the address of the *start* of this call along with its context as the restart point should this
 * process encounter a restart situation (return from $ZTRAP or outofband call typically).
 *
 * Since this is a leaf routine (makes no calls), the stack frame alignment is not important so is not adjusted
 * or tested. Should that change, the alignment should be fixed and implement use of the CHKSTKALIGN macro made.
 */
ENTRY op_restartpc
	sub	r4, lr, #8				// Xfer call size is constant, 4 for ldr and 4 for blx
	ldr	r12, [r5]				// Load content of 'frame_pointer' global var
	str	r4, [r12, #msf_restart_pc_off]		// Save restart pc in current stack frame
	ldr	r2, [r12, #msf_ctxt_off]		// Fetch frame's resume context
	str	r2, [r12, #msf_restart_ctxt_off]	// Save as the restart context in this frame as well
	bx	lr

	.end

# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
.section        .note.GNU-stack,"",@progbits
