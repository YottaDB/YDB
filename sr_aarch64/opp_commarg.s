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

/* opp_commarg.s */

/*
 * void	op_commarg(mval *v, unsigned char argcode)
 */

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.data
	.extern frame_pointer

	.text
	.extern	op_commarg


ENTRY opp_commarg
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	ldr	x27, [x19]					/* Frame before op_commarg (x27 is callee saved) */
	bl	op_commarg
	ldr	x9, [x19]
	cmp	x9, x27
	b.eq	sameframe
	/* op_commarg pushed a frame for the indirect code. Our bl stays on the return stack for the return from that
	 * frame, so branch to its code (see SFF_RSB_ENTRY in stack_frame.h).
	 */
	mark_rsb_entry
	getframe
	br	x30						/* Not ret: see the comment before the br in op_extcall.s */
sameframe:
	/* No new frame (for example an indirect GOTO to a label, or XECUTE ""). Use a ret so it consumes the entry of
	 * our bl, which no return from a frame will.
	 */
	getframe
	ret							/* CONSUMES_OWN_ENTRY (see ci/check_getframe_ret.py) */
