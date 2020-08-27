#################################################################
#								#
# Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2018 Stephen L Johnson. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

/* op_bkpt.s */

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

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
	ldr	x10, [x19]
	ldrh	w2, [x10, #msf_typ_off]
	tst	w2, #1
	b.eq	l1
	ldr	x2, =zstep_level
	ldr	x2, [x2]
	cmp	x2, x10
	b.gt	l1
	bl	op_zstepret
l1:
	b	opp_ret

ENTRY opp_zstepretarg
	CHKSTKALIGN					/* Verify stack alignment */
	mov	x27, x0					/* Save input regs */
	mov	x28, x1
	bl	op_zstepretarg_helper
	ldr	x10, [x19]
	ldr	x2, =zstep_level
	ldr	x2, [x2]
	cmp	x2, x10
	b.gt	l2
	bl	op_zstepret
l2:
	mov	x0, x27					/* Restore input regs */
	mov	x1, x28
	b	op_retarg

ENTRY op_zbfetch
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	x10, [x19]
	str	x30, [x10, #msf_mpc_off]		/* Save return address */
	mov	x27, x0					/* Save arg count */
	bl	gtm_fetch
	cmp	x27, #8
	b.le	zbfetch1
	mov	sp, x29					/* One or more args on stack, fix stack pointer */
zbfetch1:
	ldr	x0, [x19]
	bl	op_zbreak
	getframe
	ret

ENTRY op_zbstart
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	x0, [x19]
	str	x30, [x0, #msf_mpc_off]
	bl	op_zbreak
	getframe
	ret

ENTRY op_zstepfetch
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	x10, [x19]
	str	x30, [x10, #msf_mpc_off]
	mov	x27, x0					/* Save arg count */
	bl	gtm_fetch
	cmp	x27, #8
	b.le	zstep1
	mov	sp, x29					/* One or more args on stack, fix stack pointer */
zstep1:
	bl	op_zst_break
	getframe
	ret

ENTRY op_zstepstart
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	x10, [x19]
	str	x30, [x10, #msf_mpc_off]
	bl	op_zst_break
	getframe
	ret

ENTRY op_zstzbfetch
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	x10, [x19]
	str	x30, [x10, #msf_mpc_off]
	mov	x27, x0					/* Save arg count */
	bl	gtm_fetch
	cmp	x27, #8
	b.le	zstzbf1
	mov	sp, x29					/* One or more args on stack, fix stack pointer */
zstzbf1:
	ldr	x0, [x19]
	bl	op_zbreak
	bl	op_zst_break
	getframe
	ret

ENTRY op_zstzbstart
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	x0, [x19]
	str	x30, [x0, #msf_mpc_off]			/* Save return address */
	bl	op_zbreak
	bl	op_zst_break
	getframe
	ret

ENTRY op_zstzb_fet_over
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	x10, [x19]
	str	x30, [x10, #msf_mpc_off]
	mov	x27, x0					/* Save arg count */
	bl	gtm_fetch
	cmp	x27, #8
	b.le	zstzbfo1
	mov	sp, x29					/* One or more args on stack, fix stack pointer */
zstzbfo1:
	ldr	x0, [x19]
	bl	op_zbreak
	ldr	x2, =zstep_level
	ldr	x2, [x2]
	ldr	x10, [x19]
	cmp	x2, x10
	b.ge	l3
	cbnz	w0, l5
	b	l4
l3:
	bl	op_zst_break
l4:
	getframe
	ret
l5:
	bl	op_zst_over
	ldr	x2, [x19]
	ldr	x30, [x2, #msf_mpc_off]
	ret

ENTRY op_zstzb_st_over
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	x0, [x19]
	str	x30, [x0, #msf_mpc_off]
	bl	op_zbreak
	ldr	x1, =zstep_level
	ldr	x1, [x1]
	ldr	x10, [x19]
	cmp	x1, x10
	b.ge	l6
	cbnz	w0, l8
	b	l7
l6:
	bl	op_zst_break
l7:
	getframe
	ret
l8:
	bl	op_zst_over
	ldr	x10, [x19]
	ldr	x30, [x10, #msf_mpc_off]
	ret

ENTRY op_zst_fet_over
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	x10, [x19]
	str	x30, [x10, #msf_mpc_off]
	mov	x27, x0					/* Save arg count */
	bl	gtm_fetch
	cmp	x27, #8
	b.le	zstfo1
	mov	sp, x29					/* One or more args on stack, fix stack pointer */
zstfo1:
	ldr	x1, =zstep_level
	ldr	x1, [x1]
	ldr	x10, [x19]
	cmp	x1, x10
	b.gt	l9
	bl	op_zst_break
	getframe
	ret
l9:
	bl	op_zst_over
	ldr	x0, [x19]
	ldr	x30, [x0, #msf_mpc_off]
	ret

ENTRY op_zst_st_over
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	x10, [x19]
	str	x30, [x10, #msf_mpc_off]
	ldr	x2, =zstep_level
	ldr	x2, [x2]
	cmp	x2, x10
	b.gt	l10
	bl	op_zst_break
	getframe
	ret
l10:
	bl	op_zst_over
	ldr	x2, [x19]
	ldr	x30, [x2, #msf_mpc_off]
	ret

ENTRY opp_zst_over_ret
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	x10, [x19]
	ldr	x2, =zstep_level
	ldr	x2, [x2]
	ldr	x0, [x10, #msf_old_frame_off]
	cmp	x2, x0
	b.gt	l11
	bl	op_zstepret
l11:
	b	opp_ret

ENTRY opp_zst_over_retarg
	mov	x27, x0					/* Save input regs */
	mov	x28, x1
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_zst_over_retarg_helper
	ldr	x10, [x19]
	ldr	x10, [x10, #msf_old_frame_off]
	ldr	x2, =zstep_level
	ldr	x2, [x2]
	cmp	x2, x10
	b.gt	l12
	bl	op_zstepret
l12:
	mov	x0, x27					/* Restore input regs */
	mov	x1, x28
	b	op_retarg
