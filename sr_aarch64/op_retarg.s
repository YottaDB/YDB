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

/* op_retarg.s */
/*
 *	x0 - pointer to mval being returned
 *	w1 - True/False for alias return
 */

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	unw_retarg

ENTRY op_retarg
	/* Generated code branches here with b (not bl). See the comment in opp_ret.s. */
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	x9, [x19]
	ldrb	w27, [x9, #msf_flags_off]		/* Flags of the frame being unwound (x27 is callee saved) */
	bl	unw_retarg
	getframe_ret_or_br w27
