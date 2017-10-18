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

/* op_fetchintrrpt.s */

	.title	op_fetchintrrpt.s

	.include "linkage.si"
	.include "g_msf.si"
	.include "stack.si"
#	include "debug.si"

	.sbttl	op_fetchintrrpt

	.data
	.extern	frame_pointer
	.extern	neterr_pending

	.text
	.extern	gtm_fetch
	.extern	gvcmz_neterr
	.extern	outofband_clear
	.extern	async_action


ENTRY op_fetchintrrpt
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r12, [r5]
	str	lr, [r12, #msf_mpc_off]
	str	r6, [r12, #msf_ctxt_off]
	bl	gtm_fetch
	ldr	r12, =neterr_pending
	ldrb	r12, [r12]
	cmp	r12, #0
	beq	l1
	bl	outofband_clear
	mov	r0, #0
	bl	gvcmz_neterr
l1:	mov	r0, #1
	bl	async_action
	ldr	r12, [r5]
	ldr	lr, [r12, #msf_mpc_off]
	bx	lr

	.end
