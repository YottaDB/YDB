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
	/* Generated code branches here with b (not bl), so the return stack still has the entry of the bl that transferred
	 * control to the frame being unwound, if that bl left one.
	 */
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	x9, [x19]
	ldrb	w27, [x9, #msf_flags_off]		/* Flags of the frame being unwound (x27 is callee saved) */
	bl	op_unwind
	getframe_ret_or_br w27
