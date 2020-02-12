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

/* op_forcenum.s */

/*
	x1 - pointer to source mval
	x0 - pointer to destination mval
*/

	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.data
	.extern	s2n

	.text

ENTRY op_forcenum
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN				/* Verify stack alignment */
	mov	x26, x0		/* Save x0 */
	mv_force_defined x1
	mov	x27, x1		/* Save x1 */
	mv_force_num x1
	mov	x1, x27
	mov	x0, x26
	ldrh	w13, [x1, #mval_w_mvtype]
	tst	w13, #mval_m_sqlnull
	b.ne	l40			/* jump to l40 if MV_SQLNULL bit is set */
	tst	w13, #mval_m_str
	b.eq	l20			/* jump to l20 if MV_STR bit is not set */
	tst	w13, #mval_m_num_approx
	b.eq	l40			/* jump to l40 if MV_NUM_APPROX bit is not set */
l20:
	tst	w13, #mval_m_int_without_nm
	b.eq	l30
	mov	w13, #mval_m_int
	strh	w13, [x0, #mval_w_mvtype]
	ldr	w2, [x1, #mval_l_m1]
	str	w2, [x0, #mval_l_m1]
	b	done
l30:
	mov	w13, #mval_m_nm
	strh	w13, [x0, #mval_w_mvtype]
	ldrb	w13, [x1, #mval_b_exp]
	strb	w13, [x0, #mval_b_exp]

/* Copy the only numeric part of Mval from [x1] to [x0] */
	ldr	w13, [x1, #mval_l_m0]
	str	w13, [x0, #mval_l_m0]
	ldr	w13, [x1, #mval_l_m1]
	str	w13, [x0, #mval_l_m1]
	b	done
l40:
	/* Copy the Mval from [x1] to [x0] */
	mov	w11, #mval_byte_len
l50:
	ldp	x12, x13, [x1], #+16
	stp	x12, x13, [x0], #+16
	subs	w11, w11, #16
	b.gt	l50
done:
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
