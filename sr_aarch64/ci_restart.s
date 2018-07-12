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

/* ci_restart.s */
/* (re)start a GT.M frame */

/* setup register/stack arguments from 'param_list' and transfer
 *	control to op_extcall/op_extexfun which return only after the M
 *	routine finishes and QUITs.
 */

	.include "linkage.si"
	.include "stack.si"

	.extern	param_list

	.text

ci_rtn		=  0
argcnt		=  8
rtnaddr		= 24
labaddr		= 32
retaddr		= 40
mask		= 48
args		= 56

ENTRY ci_restart
	ldr	x13, =param_list
	ldr	x13, [x13]
	mov	x29, sp					/* save sp here - to be restored after op_extexfun or op_extcall */
	ldr	w9, [x13, #argcnt]			/* argcnt */
	ADJ_STACK_ALIGN_EVEN_ARGS w9
	mov	x28, sp					/* xxxxxxx Use x28 instead of sp for store since sp needs 16-byte alignment
							 * At the end, sp will be properly aligned */
	mov	w14, w9
	cmp	w9, wzr					/* if (argcnt > 0) { */
	b.le	L0
	mov	w10, w9, LSL #3				/* param_list->args[argcnt] */
	sub	w10, w10, #8
	add	x12, x13, #args				/* point at first arg */
	add	x12, x12, x10				/* point at last arg */
L1:
	ldr	w10, [x12], #-8				/* push arguments backwards to stack */
	str	w10, [x28, #-8]!
/* xxxxxxx	sub	sp, sp, #8
	str	x10, [sp] */
	subs	w9, w9, #1
	b.ne	L1					/* } */
L0:
	str	x14, [x28, #-8]!				/* push arg count on stack */
/* xxxxxxx	sub	sp, sp, #8 */				/* push arg count on stack */
	/* xxxxxxx	str	x14, [sp] */
	mov	sp, x28						/* Restore actual sp */
	ldr	x12, [x13, #mask]
	ldr	w11, [x13, #retaddr]
	ldr	w10, [x13, #labaddr]
	ldr	w9, [x13, #rtnaddr]
	ldr	x13, [x13, #ci_rtn]
	mov	x30, x13
	ret
