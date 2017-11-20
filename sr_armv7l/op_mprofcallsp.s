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

/* op_mprofcallsp.s */

	.title	op_mprofcallsp.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	op_mprofcallsp

	.data
	.extern	dollar_truth
	.extern	frame_pointer

	.text
	.extern	exfun_frame_push_dummy_frame
	.extern	push_tval

	.sbttl	op_mprofcallspb

ENTRY op_mprofcallspb
ENTRY op_mprofcallspw
ENTRY op_mprofcallspl
	push	{r4, lr}			/* r4 is to maintain 8 byte stack alignment */
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r12, [r5]
	add	r0, lr				/* Bump return pc past the branch instruction following bl that got us here */
	str	r0, [r12, #msf_mpc_off]		/* and store it in Mumps stack frame */
	bl	exfun_frame_push_dummy_frame
	ldr	r0, =dollar_truth
	ldr	r0, [r0]
	bl	push_tval
	ldr	r12, [r5]
	ldr	r9, [r12, #msf_temps_ptr_off]
	pop	{r4, pc}

	.end
