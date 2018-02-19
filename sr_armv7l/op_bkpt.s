#################################################################
#								#
# Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.	#
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

/* op_bkpt.s */

	.title	op_bkpt.s

	.include "linkage.si"
	.include "g_msf.si"
	.include "stack.si"
#	include "debug.si"

	.sbttl	opp_zstepret

	.data
	.extern	frame_pointer
	.extern	zstep_level

	.text
	.extern	gtm_fetch
	.extern	op_retarg
	.extern	op_zbreak
	.extern	op_zst_break
	.extern	op_zst_over
	.extern	op_zstepret
	.extern	opp_ret

ENTRY opp_zstepret
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r12, [r5]
	ldrh	r2, [r12, +#msf_typ_off]
	tst	r2, #1
	beq	l1
	ldr	r2, =zstep_level
	ldr	r2, [r2]
	cmp	r2, r12
	bgt	l1
	bl	op_zstepret
l1:
	b	opp_ret


	.sbttl	opp_zstepretarg

ENTRY opp_zstepretarg
	push	{r0, r1}
	CHKSTKALIGN				/* Verify stack alignment */
	bl	op_zstepretarg_helper
	ldr	r12, [r5]
	ldr	r1, =zstep_level
	ldr	r1, [r1]
	cmp	r1, r12
	bgt	l2
	bl	op_zstepret
l2:
	pop	{r0, r1}
	b	op_retarg


	.sbttl	op_zbfetch

ENTRY op_zbfetch
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r12, [r5]
	str	lr, [r12, +#msf_mpc_off]
	mov	r4, r0				/* Save arg count */
	bl	gtm_fetch
	cmp	r4, #4
	movge	sp, fp				/* One or more args on stack, fix stack pointer */
	ldr	r0, [r5]
	bl	op_zbreak
	getframe
	bx	lr


	.sbttl	op_zbstart

ENTRY op_zbstart
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r0, [r5]
	str	lr, [r0, #msf_mpc_off]
	bl	op_zbreak
	getframe
	bx	lr


	.sbttl	op_zstepfetch

ENTRY op_zstepfetch
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r12, [r5]
	str	lr, [r12, #msf_mpc_off]
	mov	r4, r0				/* Save arg count */
	bl	gtm_fetch
	cmp	r4, #4
	movge	sp, fp				/* One or more args on stack, fix stack pointer */
	bl	op_zst_break
	getframe
	bx	lr


	.sbttl	op_zstepstart

ENTRY op_zstepstart
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r12, [r5]
	str	lr, [r12, #msf_mpc_off]
	bl	op_zst_break
	getframe
	bx	lr


	.sbttl	op_zstzbfetch

ENTRY op_zstzbfetch
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r12, [r5]
	str	lr, [r12, #msf_mpc_off]
	mov	r4, r0				/* Save arg count */
	bl	gtm_fetch
	cmp	r4, #4
	movge	sp, fp				/* One or more args on stack, fix stack pointer */
	ldr	r0, [r5]
	bl	op_zbreak
	bl	op_zst_break
	getframe
	bx	lr


	.sbttl	op_zstzbstart

ENTRY op_zstzbstart
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r0, [r5]
	str	lr, [r0, #msf_mpc_off]		/* Save return address */
	bl	op_zbreak
	bl	op_zst_break
	getframe
	bx	lr


	.sbttl	op_zstzb_fet_over

ENTRY op_zstzb_fet_over
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r12, [r5]
	str	lr, [r12, #msf_mpc_off]
	mov	r4, r0				/* Save arg count */
	bl	gtm_fetch
	cmp	r4, #4
	movge	sp, fp				/* One or more args on stack, fix stack pointer */
	ldr	r0, [r5]
	bl	op_zbreak
	ldr	r2, =zstep_level
	ldr	r2, [r2]
	ldr	r12, [r5]
	cmp	r2, r12
	bge	l3
	cmp	r0, #0
	bne	l5
	b	l4
l3:
	bl	op_zst_break
l4:
	getframe
	bx	lr
l5:
	bl	op_zst_over
	ldr	r12, [r5]
	ldr	lr, [r12, #msf_mpc_off]
	bx	lr


	.sbttl	op_zstzb_st_over

ENTRY op_zstzb_st_over
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r0, [r5]
	str	lr, [r0, #msf_mpc_off]
	bl	op_zbreak
	ldr	r1, =zstep_level
	ldr	r1, [r1]
	ldr	r12, [r5]
	cmp	r1, r12
	bge	l6
	cmp	r0, #0
	bne	l8
	b	l7
l6:
	bl	op_zst_break
l7:
	getframe
	bx	lr
l8:
	bl	op_zst_over
	ldr	r12, [r5]
	ldr	lr, [r12, #msf_mpc_off]
	bx	lr


	.sbttl	op_zst_fet_over

ENTRY op_zst_fet_over
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r12, [r5]
	str	lr, [r12, #msf_mpc_off]
	mov	r4, r0				/* Save arg count */
	bl	gtm_fetch
	cmp	r4, #4
	movge	sp, fp				/* One or more args on stack, fix stack pointer */
	ldr	r1, =zstep_level
	ldr	r1, [r1]
	ldr	r12, [r5]
	cmp	r1, r12
	bgt	l9
	bl	op_zst_break
	getframe
	bx	lr
l9:
	bl	op_zst_over
	ldr	r12, [r5]
	ldr	lr, [r12, #msf_mpc_off]
	bx	lr


	.sbttl	op_zst_st_over

ENTRY op_zst_st_over
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r12, [r5]
	str	lr, [r12, #msf_mpc_off]
	ldr	r1, =zstep_level
	ldr	r1, [r1]
	cmp	r1, r12
	bgt	l10
	bl	op_zst_break
	getframe
	bx	lr
l10:
	bl	op_zst_over
	ldr	r12, [r5]
	ldr	lr, [r12, #msf_mpc_off]
	bx	lr


	.sbttl	opp_zst_over_ret

ENTRY opp_zst_over_ret
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r12, [r5]
	ldrh	r1, [r12, #msf_typ_off]
	tst	r1, #1
	beq	l11
	ldr	r1, =zstep_level
	ldr	r1, [r1]
	ldr	r0, [r12, #msf_old_frame_off]
	cmp	r1, r0
	bgt	l11
	bl	op_zstepret
l11:
	b	opp_ret


	.sbttl	opp_zst_over_retarg

ENTRY opp_zst_over_retarg
	push	{r0, r1}
	CHKSTKALIGN				/* Verify stack alignment */
	bl	op_zst_over_retarg_helper
	ldr	r12, [r5]
	ldr	r0, [r12, #msf_old_frame_off]
	ldr	r1, =zstep_level
	ldr	r1, [r1]
	cmp	r1, r0
	bgt	l12
	bl	op_zstepret
l12:
	pop	{r0, r1}
	b	op_retarg

	.end
