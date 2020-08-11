#################################################################
#								#
# Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	#
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

/* op_neg.s */
/*

	op_neg ( mval *u, mval *v ) : u = -v

	    x1 - source mval      = &v
	    x0 - destination mval = &u
*/

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.text
	.extern	s2n
	.extern underr

ENTRY op_neg
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN					/* Verify stack alignment */

	mov	x27, x0
	mv_force_defined x1
	ldrh	w13, [x1, #mval_w_mvtype]
	tst	w13, #mval_m_sqlnull
	b.ne	sqlnull					/* jump to sqlnull if MV_SQLNULL bit is set */
	mv_if_number x1, numer
	mov	x28, x1
	mov	x0, x1
	bl	s2n
	mov	x1, x28
numer:
	mov	x0, x27
	mv_if_notint x1, float
	mov	w2, #mval_m_int
	strh	w2, [x0, #mval_w_mvtype]
	ldr	x1, [x1, #mval_l_m1]
	sub	x1, xzr, x1
	str	x1, [x0, #mval_l_m1]
	b	done
sqlnull:
	/* Copy the Mval from [x1] to [x0] */
	mov	w11, #mval_byte_len
mvalcopy:
	ldp	x12, x13, [x1], #+16
	stp	x12, x13, [x0], #+16
	subs	w11, w11, #16
	b.gt	mvalcopy
	b	done
float:
	mov	w3, #mval_m_nm
	strh	w3, [x0, #mval_w_mvtype]
	ldrb	w3, [x1, #mval_b_exp]
	eor	w3, w3, #mval_esign_mask			/* flip the sign bit */
	strb	w3, [x0, #mval_b_exp]
	ldr	x3, [x1, #mval_l_m0]
	str	x3, [x0, #mval_l_m0]
	ldr	x3, [x1, #mval_l_m1]
	str	x3, [x0, #mval_l_m1]
done:
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
