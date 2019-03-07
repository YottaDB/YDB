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

/* op_mprofforlcldo.s */

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	exfun_frame_sp

ENTRY op_mprofforlcldob
ENTRY op_mprofforlcldol
ENTRY op_mprofforlcldow
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	x26, [x19]
	add	x9, x30, x0
	str	x9, [x26, #msf_mpc_off]			/* store adjusted return address in MUMPS stack frame */
	bl	exfun_frame_sp
	ldr	x26, [x19]
	ldr	x21, [x26, #msf_temps_ptr_off]
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret

