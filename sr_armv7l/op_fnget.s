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

/* op_fnget.s */

/*
	OP_FNGET implements the $Get() function.
	Upon entry, r1 -> source mval, r0 -> destination mval.
	If the source mval is defined, it is copied to the target,
	otherwise, the target mval is set to be a null string.
*/

	.title	op_fnget.s

	.include "linkage.si"
	.include "mval_def.si"

	.sbttl	op_fnget

/*
 * Note there is no stack padding for alignment and no check in this routine because it is a leaf routine
 * so never calls anything else. That is not an issue unless this routine calls something in the future in
 * which case it needs changes to pad the stack for alignment and should then also use the CHKSTKALIGN macro
 * to verify it.
 */
ENTRY op_fnget
	push	{r6, lr}
	cmp	r1, #0
	beq	l5				/* If arg = 0, set type and len */
	mov	r6, r0
	mv_if_notdefined r1, l5
	/* Copy the mval from [r1] to [r0] */
	mov	r0, r6
	mov	r2, #mval_byte_len		/* Assumption: mval_byte_len > 0 */
l1:	ldr	r3, [r1], #+4
	str	r3, [r0], #+4
	subs	r2, #4				/* Just copied 4 bytes */
	bne	l1
l2:	mov	r1, #mval_m_aliascont		/* Don't propagate alias container flag */
	mvn	r1, r1
	ldrh	r0, [r6, #mval_w_mvtype]
	and	r0, r1
	strh	r0, [r6, #mval_w_mvtype]
	pop	{r6, pc}

l5:	mov	r1, #mval_m_str
	strh	r1, [r0, #mval_w_mvtype]	/* string type */
	mov	r1, #0
	str	r1, [r0, #mval_l_strlen]	/* dest str len = 0 */
	pop	{r6, pc}

	.end
