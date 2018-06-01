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

/* op_forintrrpt.s */

	.include "g_msf.si"
	.include "linkage.si"
#	include "debug.si"

	.data
	.extern	neterr_pending
	.extern	restart_pc

	.text
	.extern	gvcmz_neterr
	.extern	async_action
	.extern	outofband_clear

 
ENTRY op_forintrrpt
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	x2, =neterr_pending
	ldrb	w9, [x2]
	cbz	w9, l1
	bl	outofband_clear
	mov	x0, xzr
	bl	gvcmz_neterr
l1:
	mov	w0, wzr
	bl	async_action
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
