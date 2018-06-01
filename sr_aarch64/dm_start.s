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
	sub	sp, sp, #96
	stp	x19, x20, [sp]				/* Save registers r19 - r30 */
	stp	x21, x22, [sp, #16]
	stp	x23, x24, [sp, #32]
	stp	x25, x26, [sp, #48]
	stp	x27, x28, [sp, #64]
	stp	x29, x30, [sp, #80]
	mov	x29, sp
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	x9, =mumps_status
	mov	w10, #1
	str	w10, [x9]				/* Init mumps_status to 1 */
	ldr	x23, =xfer_table			/* x23 GTM_REG_XFER_TABLE */
	ldr	x19, =frame_pointer			/* X19 REG_FRAME_POINTER */
	ldr	x25, =dollar_truth			/* x25 GTM_REG_DOLLAR_TRUTH */
	str	w10, [x25]				/* Init $T to 1 */
	ESTABLISH

	ldr	x10, =restart
	ldr	x10, [x10]
	blr	x10
return:
	ldr	x9, =mumps_status
	ldr	w0, [x9]
	mov	sp, x29
	ldp	x19, x20, [sp]				/* Save registers r19 - r28 */
	ldp	x21, x22, [sp, #16]
	ldp	x23, x24, [sp, #32]
	ldp	x25, x26, [sp, #48]
	ldp	x27, x28, [sp, #64]
	ldp	x29, x30, [sp, #80]
	add	sp, sp, #96
	ret

ENTRY gtm_ret_code
	CHKSTKALIGN					/* Verify stack alignment */
	REVERT
	bl	op_unwind
	ldr	x9, =msp
	ldr	x11, [x9]
	ldr	x12, [x11]
	str	x12, [x19]				/* frame_pointer <-- msp */
	add	x11, x11, #8
	str	x11, [x9]				/* msp += 8 */
	b	return

ENTRY gtm_levl_ret_code
	CHKSTKALIGN					/* Verify stack alignment */
	REVERT
	b	return
