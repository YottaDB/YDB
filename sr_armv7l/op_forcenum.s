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

/* op_forcenum.s */

/*
	r1 - pointer to source mval
	r0 - pointer to destination mval
*/

	.title	op_forcenum.s

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.sbttl	op_forcenum

	.data
	.extern	s2n

	.text
save_r1		= -8
save_r0		= -4
FRAME_SIZE	=  8

ENTRY op_forcenum
	push	{fp, lr}
	mov	fp, sp
	sub	sp, sp, #FRAME_SIZE
	CHKSTKALIGN				/* Verify stack alignment */
	str	r0, [fp, #save_r0]		/* Save r0 */
	mv_force_defined r1
	str	r1, [fp, #save_r1]		/* Save r1 */
	mv_force_num r1
	ldr	r1, [fp, #save_r1]
	ldr	r0, [fp, #save_r0]
	ldrh	r4, [r1, #mval_w_mvtype]
	tst	r4, #mval_m_str
	beq	l20
	tst	r4, #mval_m_num_approx
	beq	l40
l20:
	tst	r4, #mval_m_int_without_nm
	beq	l30
	mov	r4, #mval_m_int
	strh	r4, [r0, #mval_w_mvtype]
	ldr	r2, [r1, #mval_l_m1]
	str	r2, [r0, #mval_l_m1]
	b	done
l30:	mov	r4, #mval_m_nm
	strh	r4, [r0, #mval_w_mvtype]
	ldrb	r4, [r1, #mval_b_exp]
	strb	r4, [r0, #mval_b_exp]

/* Copy the only numeric part of Mval from [r1] to [r0] */
	ldr	r4, [r1, #mval_l_m0]
	str	r4, [r0, #mval_l_m0]
	ldr	r4, [r1, #mval_l_m1]
	str	r4, [r0, #mval_l_m1]
	b	done
l40:
	/* Copy the Mval from [r1] to [r0] */
	mov	r2, #mval_byte_len
l50:
	ldr	r3, [r1], #+4
	str	r3, [r0], #+4
	subs	r2, #4
	bgt	l50
done:
	mov	sp, fp
	pop	{fp, pc}

	.end
