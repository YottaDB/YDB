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

/* mum_tstart.s */
/* (re)start a GT.M stack frame
 *
 *	mum_tstart calls trans_code if proc_act_type is non-zero.  Then
 *	mum_tstart (re)loads the GT.M registers (including the code address)
 *	from the GT.M MUMPS stack frame and then jumps to the code address indicated
 *	by the GT.M MUMPS stack frame.
 */

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.data
	.extern	frame_pointer
	.extern	proc_act_type
	.extern xfer_table

	.text
	.extern	trans_code

ENTRY mum_tstart
	CHKSTKALIGN				/* Verify stack alignment */
	ldr	x9, =proc_act_type
	ldrh	w10, [x9]
	cbz	x10, l1
	bl	trans_code
l1:
	getframe
	ldr	x23, =xfer_table
	ldr	x19, =frame_pointer
	ret
