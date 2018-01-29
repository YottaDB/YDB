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

/* op_exfun.s */

/*
	op_exfun - invoke (internal) extrinsic function

	arguments:
	    r0 - ret_value_addr	address where function places return value
	    r1 - offset		offset from our return address to where this stackframe needs to return
	    r2 - mask arg
	    r3 - actualcnt	actual argument count
	  the rest of the args are on the stack
		actual1		address of actual first argument
		actual2		address of actual second argument
		. . .
*/

	.title	op_exfun.s

	.include "linkage.si"
	.include "g_msf.si"
	.include "stack.si"
#	include "debug.si"

	.sbttl	op_exfun

	.data
	.extern	dollar_truth
	.extern	frame_pointer

	.text
	.extern	exfun_frame
	.extern	push_parm
	.extern	rts_error

act_cnt		=	-12
mask_arg	=	 -8
ret_val		=	 -4
FRAME_SAVE_SIZE	=	 16				/* Multiple of 8 to ensure stack alignment */

ENTRY op_exfun
	push	{fp, lr}
	mov	fp, sp
	sub	sp, #FRAME_SAVE_SIZE			/* establish space for locals */
	CHKSTKALIGN					/* Verify stack alignment */
	str	r0, [fp, #ret_val]			/* save incoming arguments */
	str	r2, [fp, #mask_arg]
	str	r3, [fp, #act_cnt]

	cmp	r3, #0
	addne	lr, #4					/* if one or more args, a "mov sp, fp" to skip */

	ldr	r4, [lr]				/* verify the instruction immediately after return */
	lsr	r4, r4, #24
	cmp	r4, #0xea				/* The instruction is a short branch */
	beq	inst_ok
	/*
	 * The instructions might be a long branch
	 */
	ldr	r4, [lr]
	ldr	r12, =0xe1a0c00f			/* mov  r12, pc */
	cmp	r4, r12
	bne	error
	ldr	r4, [lr, #4]
	ldr	r12, =0xe51f4000			/* ldr  r4, [pc] */
	cmp	r4, r12
	bne	error
	ldr	r4, [lr, #8]
	ldr	r12, =0xea000000			/* b  pc */
        cmp     r4, r12
	beq	inst_ok
error:
	ldr	r1, =ERR_GTMCHECK
	ldr	r1, [r1]
	mov	r0, #1
	bl	rts_error
	b	retlab
inst_ok:
	ldr	r12, [r5]
	add	r1, lr
	str	r1, [r12, #msf_mpc_off]
	bl	exfun_frame

	ldr	r0, [fp, #act_cnt]			/* number of arguments to copy */
	ADJ_STACK_ALIGN_EVEN_ARGS r0
	movs	r12, r0
	beq	done
	add	r1, fp, r12, LSL #2			/* where to copy from -- skip 4 * (count - 1) + 8 */
	add	r1, #4					/* the 8 is for the pushed lr and fp, so 4 * count + 4 */
loop:
	ldr	r3, [r1], #-4
	push	{r3}
	subs	r12, #1
	bne	loop
done:
	push	{r0}					/* actual count */
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	r3, [fp, #mask_arg]
	ldr	r2, [fp, #ret_val]
	ldr	r1, =dollar_truth
	ldr	r1, [r1]
	and	r1, #1
	add	r0, #4
	bl	push_parm				/* push_parm (total_cnt, $T, ret_value, mask, argc [,arg1, arg2, ...]); */
	ldr	r12, [r5]
	ldr	r9, [r12, #msf_temps_ptr_off]		/* GTM_REG_FRAME_TMP_PTR */
retlab:
	mov	sp, fp					/* sp is back where it was just after push at entry */
	pop	{fp, pc}				/* now fp is value on entry */

gtmcheck:
	ldr	r1, =#ERR_GTMCHECK
	mov	r0, #1
	bl	rts_error
	b	retlab

	.end
