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

/* callsp.s
 *
 *
 * op_callsp - Used to build a new stack level for argumentless DO (also saves $TEST)
 *
 * Argument:
 *	r0 - Value from OCNT_REF triple that contains the byte offset from the return address
 *	     to return to when the level pops.
 *
 * Note this routine calls exfun_frame() instead of copy_stack_frame() because this routine needs to provide a
 * separate set of compiler temps for use by the new frame. Particularly when it called on same line with FOR.
 */

	.title	op_callsp.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	op_callsp
	.data
	.extern	dollar_truth
	.extern	frame_pointer

	.text
	.extern	exfun_frame
	.extern	push_tval

	.sbttl	op_callspb, op_callspw, op_callspl

ENTRY op_callspl
ENTRY op_callspw
ENTRY op_callspb
	push	{r4, lr}			/* r4 is to maintain 8 byte stack alignment */
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r12, [r5]
	add	r0, lr				/* Add return offset to return addr */
	str	r0, [r12, #msf_mpc_off]		/* and store it in Mumps stack frame */
	bl	exfun_frame			/* Copies stack frame and creates new temps */
	ldr	r0, =dollar_truth
	ldr	r0, [r0]
	bl	push_tval
	ldr	r12, [r5]
	ldr	r9, [r12, #msf_temps_ptr_off]
	pop	{r4, pc}

	.end
