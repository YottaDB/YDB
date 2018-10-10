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

/* op_mprofcallsp.s */

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.data
	.extern	dollar_truth
	.extern	frame_pointer

	.text
	.extern	exfun_frame_push_dummy_frame
	.extern	push_tval

ENTRY op_mprofcallspb
ENTRY op_mprofcallspw
ENTRY op_mprofcallspl
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	x10, [x19]
	add	x2, x0, x30			/* Add return offset to return addr */
	str	x2, [x10, #msf_mpc_off]		/* and store it in Mumps stack frame */
	bl	exfun_frame_push_dummy_frame
	ldr	w0, [x25]
	bl	push_tval
	ldr	x10, [x19]
	ldr	x21, [x10, #msf_temps_ptr_off]
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
