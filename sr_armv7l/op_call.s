#################################################################
#								#
# Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2017-2018 Stephen L Johnson.			#
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
#	include "debug.si"

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
	ldr	r4, [lr]			/* verify the instruction immediately after return */
	lsr	r4, r4, #24
	cmp	r4, #0xea			/* Is the instruction a (short) branch */
	addeq	r0, lr, #4			/* Bump return pc past the short branch instruction */
	addne	r0, lr, #24			/* Bump return pc past the long branch instruction */
	str	r0, [r12, #msf_mpc_off]		/* and store it in Mumps stack frame */
	bl	copy_stack_frame
	pop	{r4, pc}

.end
