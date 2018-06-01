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

/* op_mproflinefetch.s */

	.include "linkage.si"
	.include "g_msf.si"
	.include "stack.si"
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	gtm_fetch
	.extern	stack_leak_check
	.extern pcurrpos

ENTRY op_mproflinefetch
	CHKSTKALIGN						/* Verify stack alignment */
	mov	x15, x0						/* Save arg count */
	ldr	x27, [x19]
	str	x30, [x27, #msf_mpc_off]			/* save return address in frame_pointer->mpc */
	str	x24, [x27, #msf_ctxt_off]			/* save linkage pointer */
	bl	gtm_fetch
	bl	pcurrpos
	cmp	x15, #8						/* Any args pushed on stack? */
	b.le	b1
	mov	sp, x29						/* If args on stack, we need to put sp where it was before
								 * calling op_linefetch - otherwise stack_leak_check will complain.
								 * But after returning to generated code, the stack pointer will be
								 * put back where it belongs
								 */
b1:	
	bl	stack_leak_check
	ldr	x27, [x19]
	ldr	x30, [x27, #msf_mpc_off]
	ret
