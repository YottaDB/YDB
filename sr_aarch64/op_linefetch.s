#################################################################
#								#
# Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	#
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

/* op_linefetch.s */

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	gtm_fetch

ENTRY op_linefetch
	CHKSTKALIGN						/* Verify stack alignment */
	ldr	x27, [x19]
	str	x30, [x27, #msf_mpc_off]			/* save return address in frame_pointer->mpc */
	str	x24, [x27, #msf_ctxt_off]			/* save linkage pointer */
	bl	gtm_fetch
	ldr	x27, [x19]
	ldr	x30, [x27, #msf_mpc_off]
	ret

