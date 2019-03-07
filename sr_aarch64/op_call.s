#################################################################
#								#
# Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	#
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

/* call.s */

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	copy_stack_frame

/*
 * op_call - Sets up a local routine call (does not leave routine)
 *
 * Argument:
 *	x0 - Value from OCNT_REF triple that contains the byte offset from the return address
 *	     where the local call should actually return to.
 */

ENTRY op_calll
ENTRY op_callw
ENTRY op_callb
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN					/* Verify stack alignment */
	add	x2, x30, x0				/* Bump return pc past the branch instruction */
	ldr	x9, [x19]
	str	x2, [x9, #msf_mpc_off]			/* and store it in Mumps stack frame */
	bl	copy_stack_frame
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
