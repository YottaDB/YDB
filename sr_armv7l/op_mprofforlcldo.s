#################################################################
#								#
# Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

/* op_mprofforlcldo.s */

	.title	op_mprofforlcldo.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	op_mprofforlcldo

	.data
.extern	frame_pointer

	.text
.extern	exfun_frame_sp

	.sbttl	op_mprofforlcldob

ENTRY op_mprofforlcldob
ENTRY op_mprofforlcldol
ENTRY op_mprofforlcldow
	push	{r4, lr}				/* r4 is to maintain 8 byte stack alignment */
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	r12, [r5]
	mov	r4, lr					/* return address */
	add	r4, r0
	str	r4, [r12, #msf_mpc_off]			/* store adjusted return address in MUMPS stack frame */
	bl	exfun_frame_sp
	ldr	r12, [r5]
	ldr	r9, [r12, #msf_temps_ptr_off]
	pop	{r4, pc}

.end
