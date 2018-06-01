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

	.include "linkage.si"
	.include "g_msf.si"
	.include "stack.si"
#	include "debug.si"

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
	ldr	x15, [x19]
	str	x30, [x15, #msf_mpc_off]
	str	x24, [x15, #msf_ctxt_off]
	bl	gtm_fetch
	ldr	x15, =neterr_pending
	ldrb	w15, [x15]
	cbz	w15, l1
	bl	outofband_clear
	mov	X0, xzr
	bl	gvcmz_neterr
l1:
	mov	x0, #1
	bl	async_action
	ldr	x15, [x19]
	ldr	x30, [x15, #msf_mpc_off]
	ret

