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

/* callsp.s
 *
 *
 * op_callsp - Used to build a new stack level for argumentless DO (also saves $TEST)
 *
 * Argument:
 *	x0 - Value from OCNT_REF triple that contains the byte offset from the return address
 *	     to return to when the level pops.
 *
 * Note this routine calls exfun_frame() instead of copy_stack_frame() because this routine needs to provide a
 * separate set of compiler temps for use by the new frame. Particularly when it called on same line with FOR.
 */

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.data
	.extern	dollar_truth
	.extern	frame_pointer

	.text
	.extern	exfun_frame
	.extern	push_tval

ENTRY op_callspl
ENTRY op_callspw
ENTRY op_callspb
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	x10, [x19]
	add	x2, x0, x30			/* Add return offset to return addr */
	str	x2, [x10, #msf_mpc_off]		/* and store it in Mumps stack frame */
	bl	exfun_frame			/* Copies stack frame and creates new temps */
	ldr	w0, [x25]
	bl	push_tval
	ldr	x10, [x19]
	ldr	x21, [x10, #msf_temps_ptr_off]
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
