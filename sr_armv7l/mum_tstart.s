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

/* mum_tstart.s */
/* (re)start a GT.M stack frame
 *
 *	mum_tstart calls trans_code if proc_act_type is non-zero.  Then
 *	mum_tstart (re)loads the GT.M registers (including the code address)
 *	from the GT.M MUMPS stack frame and then jumps to the code address indicated
 *	by the GT.M MUMPS stack frame.
 */

	.title	mum_tstart.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	mum_tstart

	.data
	.extern	frame_pointer
	.extern	proc_act_type
	.extern xfer_table

	.text
	.extern	trans_code

ENTRY mum_tstart
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	r0, =proc_act_type
	ldrh	r0, [r0]
	cmp	r0, #0
	beq	l1
	bl	trans_code
l1:
	getframe
	ldr	r7, =xfer_table
	ldr	r5, =frame_pointer
	bx	lr

	.end
