#################################################################
#								#
# Copyright (c) 2018-2026 YottaDB LLC and/or its subsidiaries.	#
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

/* opp_ret.s */

/*
 * void op_unwind(void)
 */

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	op_unwind

ENTRY opp_ret
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_unwind
	getframe
	/* x30 holds the M code address of the frame (set by getframe), not the address after the bl that called this
	 * routine. A ret would be predicted from the return stack, which holds the latter, so it would be mispredicted
	 * every time. A br is predicted from the targets this br jumped to before, and so is less likely to be mispredicted.
	 */
	br	x30
