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

/* op_mprofexfun.s */

/*
	call op_mprofexfun with the following registers:

		x0 - ret_value_addr	address where function places return value
		x1 - offset		offset from our return address to where this stackframe needs to return
		w2 - mask_arg
		w3 - actualcnt		actual argument count
		x4 - actual1		address of actual first argument
		x5 - actual2		address of actual second argument
		x6 - actual3		address of actual third argument
		x7 - actual4		address of actual fourth argument
	  the rest of the args are on the stack
		actual5			address of actual fifth argument
		actual6			address of actual sixth argument
		. . .
*/

	.include "linkage.si"
	.include "g_msf.si"
	.include "stack.si"
#	include "debug.si"

	.data
	.extern	dollar_truth

	.text
	.extern	exfun_frame_sp
	.extern	push_parm
	.extern	rts_error

actual4		=	-48
actual3		=	-40
actual2		=	-32
actual1		=	-24
act_cnt		=	-16
mask_arg	=	-12
ret_val		=	 -8
FRAME_SAVE_SIZE	=	 48				/* Multiple of 16 to ensure stack alignment */

ENTRY op_mprofexfun
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	sub	sp, sp, #FRAME_SAVE_SIZE		/* Establish space for locals */
	CHKSTKALIGN					/* Verify stack alignment */
	str	x0, [x29, #ret_val]			/* save incoming arguments */
	str	w2, [x29, #mask_arg]
	str	w3, [x29, #act_cnt]
	str	x4, [x29, #actual1]
	str	x5, [x29, #actual2]
	str	x6, [x29, #actual3]
	str	x7, [x29, #actual4]

	cmp	w3, #4
	b.le	no_stack_args
	add	x30, x30, #4				/* Five or more args, something is on stack - need to skip "mov sp, x29" */
no_stack_args:
	ldr	w13, [x30]				/* Verify the instruction immediately after return */
	lsr	w13, w13, #26				/* Branch op is in bits 26 - 31 */
	cmp	w13, #0x05				/* The instruction is a branch */
	b.ne	gtmcheck

	ldr	x27, [x19]
	add	x1, x1, x30
	str	x1, [x27, #msf_mpc_off]
	bl	exfun_frame_sp
	ldr	w0, [x29, #act_cnt]			/* Number of arguments */
	cbz	w0, no_arg
	cmp	w0, #1
	b.eq	one_arg
	cmp	w0, #2
	b.eq	two_arg
	cmp	w0, #3
	b.eq	three_arg
	ADJ_STACK_ALIGN_EVEN_ARGS w0
	mov	x11, sp					/* Use x11 for stack copy since 8 byte alignment is bad for sp */
	sub	w9, w0, #4				/* Number of arguments to copy (number already pushed on stack) */
	cmp	w0, #4
	b.eq	four_arg
	add	x10, x29, x9, LSL #3			/* Where to copy from -- skip 8 * (count - 1) + 16 */
	add	x10, x10, #8				/* The 16 is for the pushed x30 and x29, so 8 * count + 8 */
loop:
	ldr	x12, [x10], #-8
	str	x12, [x11, #-8]!
	subs	w9, w9, #1
	b.ne	loop
four_arg:
	ldr	x12, [x29, #actual4]
	str	x12, [x11, #-8]!
	mov	sp, x11					/* Copy is done, now we can use sp */
	CHKSTKALIGN					/* Verify stack alignment */
three_arg:
	ldr	x7, [x29, #actual3]
two_arg:
	ldr	x6, [x29, #actual2]
one_arg:
	ldr	x5, [x29, #actual1]
no_arg:	
	ldr	w4, [x29, #act_cnt]
	ldr	w3, [x29, #mask_arg]
	ldr	x2, [x29, #ret_val]
	ldr	w1, [x25]				/* Dollar truth */
	and	w1, w1, #1
	add	w0, w0, #4
	bl	push_parm				/* push_parm (total_cnt, $T, ret_value, mask, argc [,arg1, arg2, ...]); */
	ldr	x27, [x19]
	ldr	x21, [x27, #msf_temps_ptr_off]		/* GTM_REG_FRAME_TMP_PTR */
retlab:
	mov	sp, x29					/* sp is back where it was just after push at entry */
	ldp	x29, x30, [sp], #16
	ret

gtmcheck:
	ldr	x1, =ERR_GTMCHECK
	mov	x0, #1
	bl	rts_error
	b	retlab
