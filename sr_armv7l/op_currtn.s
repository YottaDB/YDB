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

/* op_currtn.s */

	.include "linkage.si"
	.include "mval_def.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	op_currtn
	.data
	.extern frame_pointer

	.text

/*
 * Routine to fill in an mval with the current routine name.
 *
 * Note since this routine makes no calls, stack alignment is not critical. If ever a call is added then this
 * routine should take care to align the stack to 8 bytes and add a CHKSTKALIGN macro.
 */
ENTRY op_currtn
	mov	r0, #mval_m_str
	strh	r0, [r1, #mval_w_mvtype]
	ldr	r12, [r5]
	ldr	r0, [r12, #msf_rvector_off]
	ldr	r4, [r0, #mrt_rtn_len]
	str	r4, [r1, #mval_l_strlen]	/* r1->str.len = frame_pointer->rvector->routine_name.len */
	ldr	r4, [r0, #mrt_rtn_addr]
	str	r4, [r1, #mval_a_straddr]	/* r1->str.addr = frame_pointer->rvector->routine_name.addr */
	bx	lr

	.end
