#################################################################
#								#
# Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries.	#
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

/* op_mproflinefetch.s */

	.title	op_mproflinefetch.s

	.include "linkage.si"
	.include "g_msf.si"
	.include "stack.si"
#	include "debug.si"

	.sbttl	op_mproflinefetch

	.data
	.extern	frame_pointer

	.text
	.extern	gtm_fetch
	.extern pcurrpos

ENTRY op_mproflinefetch
	CHKSTKALIGN					/* Verify stack alignment */
	mov	r4, r0					/* Save arg count */
	ldr	r12, [r5]
	str	lr, [r12, #msf_mpc_off]			/* save return address in frame_pointer->mpc */
	str	r6, [r12, #msf_ctxt_off]		/* save linkage pointer */
	bl	gtm_fetch
	bl	pcurrpos
	ldr	r12, [r5]
	ldr	lr, [r12, #msf_mpc_off]
	bx	lr

	.end

# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
.section        .note.GNU-stack,"",@progbits
