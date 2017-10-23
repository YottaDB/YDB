#################################################################
#								#
# Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

/* call.s */


	.title	op_call.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	op_callb, op_callw, op_calll

	.data
.extern	frame_pointer

	.text
.extern	copy_stack_frame


ENTRY op_calll
ENTRY op_callw
ENTRY op_callb
	push	{r4, lr}			/* r4 is to maintain 8 byte stack alignment */
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r12, [r5]
	add	r0, lr, #4			/* Bump return pc past the branch instruction following bl that got us here */
	str	r0, [r12, #msf_mpc_off]		/* and store it in Mumps stack frame */
	bl	copy_stack_frame
	pop	{r4, pc}

.end
