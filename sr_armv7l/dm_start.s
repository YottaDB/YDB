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

/* dm_start.s */
/* int dm_start() */

	.title	dm_start.s
	.sbttl	dm_start

	.include "linkage.si"
	.include "error.si"
	.include "stack.si"
#	include "debug.si"

	.data
	.extern	frame_pointer
	.extern	dollar_truth
	.extern	xfer_table
	.extern	msp
	.extern	mumps_status
	.extern	restart

	.text
	.extern	mdb_condition_handler
	.extern	op_unwind
	.extern __sigsetjmp				/* setjmp() is really __sigsetjmp(env,0) */
	.extern rts_error

ENTRY dm_start
	stmfd   sp!, {r4, r5, r6, r7, r8, r9, r10, fp, r12, lr}
	mov	fp, sp
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r0, =mumps_status
	mov	r1, #1
	str	r1, [r0]
	ldr	r7, =xfer_table
	ldr	r5, =frame_pointer
	ldr	r0, =dollar_truth
	str	r1, [r0]
	ESTABLISH

	ldr	r0, =restart
	ldr	r0, [r0]
	blx	r0
return:
	ldr	r0, =mumps_status
	ldr	r0, [r0]
	mov	sp, fp
	ldmfd   sp!, {r4, r5, r6, r7, r8, r9, r10, fp, r12, pc}


	.sbttl	gtm_ret_code

ENTRY gtm_ret_code
	CHKSTKALIGN				/* Verify stack alignment */
	REVERT
	bl	op_unwind
	ldr	r0, =msp
	ldr	r2, [r0]
	ldr	r3, [r2]
	str	r3, [r5]			/* frame_pointer <-- msp */
	add	r2, #4
	str	r2, [r0]			/* msp += 4 */
	b	return

	.sbttl	gtm_levl_ret_code

/* Used by triggers and call-ins to return from a nested generated code call
 * (a call not at the base C stack level) without doing an unwind.
 */
ENTRY gtm_levl_ret_code
	CHKSTKALIGN				/* Verify stack alignment */
	REVERT
	b	return

	.end
