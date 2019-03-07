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

/* op_fnget.s */

/* 
	OP_FNGET implements the $Get() function.
	Upon entry, REG64_ARG1 -> source mval, REG64_ARG0 -> destination mval.
	If the source mval is defined, it is copied to the target,
	otherwise, the target mval is set to be a null string.
*/


	.include "linkage.si"
	.include "mval_def.si"
 
/*
 * Note there is no stack padding for alignment and no check in this routine because it is a leaf routine
 * so never calls anything else. That is not an issue unless this routine calls something in the future in
 * which case it needs changes to pad the stack for alignment and should then also use the CHKSTKALIGN macro
 * to verify it.
 */
ENTRY op_fnget
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	cbz	x1, l5					/* If arg = 0, set type and len */
	mov	x9, x0
	mv_if_notdefined x1, l5

/* Copy the mval from [x1] to [x0] */
	mov	x0, x9
	mov	w11, #mval_byte_len			/* Assumption: mval_byte_len > 0 */
l1:	ldp	x12, x13, [x1], #+16
	stp	x12, x13, [x0], #+16
	subs	w11, w11, #16				/* Just copied 16 bytes */
	b.ne	l1
l2:	mov	w1, #mval_m_aliascont			/* Don't propagate alias container flag */
	mvn	w1, w1
	ldrh	w0, [x9, #mval_w_mvtype]
	and	w0, w0, w1
	strh	w0, [x9, #mval_w_mvtype]
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret

l5:	mov	w1, #mval_m_str
	strh	w1, [x0, #mval_w_mvtype]		/* string type */
	mov	x1, xzr
	str	x1, [x0, #mval_l_strlen]		/* dest str len = 0 */
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret

