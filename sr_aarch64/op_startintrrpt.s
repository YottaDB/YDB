#################################################################
#								#
# Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	#
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

/* op_startintrrpt.s */

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.data
	.extern	frame_pointer
	.extern	neterr_pending

	.text
	.extern	gvcmz_neterr
	.extern	async_action
	.extern	outofband_clear

ENTRY op_startintrrpt
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	x0, =neterr_pending
	ldrb	w0, [x0]
	cbz	w0, l1
	bl	outofband_clear
	mov	x0, xzr
	bl	gvcmz_neterr
l1:
	mov	w0, #1
	bl	async_action
	getframe
	ret
