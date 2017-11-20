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

/* op_neg.s */
/*

	op_neg ( mval *u, mval *v ) : u = -v

	    r1 - source mval      = &v
	    r0 - destination mval = &u
*/

	.title	op_neg.s

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.sbttl	op_neg

	.text
	.extern	s2n
	.extern underr

save_ret1	= -8
save_ret0	= -4
FRAME_SIZE	=  8

ENTRY op_neg
	push	{fp, lr}
	mov	fp, sp
	sub	sp, #FRAME_SIZE				/* Stack remains 8 byte aligned */
	CHKSTKALIGN					/* Verify stack alignment */
 	str	r0, [fp, #save_ret0]
	mv_force_defined r1
	mv_if_number r1, numer
	str	r1, [fp, #save_ret1]
	mov	r0, r1
	bl	s2n
	ldr	r1, [fp, #save_ret1]
numer:
	ldr	r0, [fp, #save_ret0]
	mv_if_notint r1, float
	mov	r2, #mval_m_int
	strh	r2, [r0, #mval_w_mvtype]
	ldr	r1, [r1, #mval_l_m1]
	rsb	r1, r1, #0
	str	r1, [r0, #mval_l_m1]
	b	done
float:
	mov	r3, #mval_m_nm
	strh	r3, [r0, #mval_w_mvtype]
	ldrb	r3, [r1, #mval_b_exp]
	eor	r3, #mval_esign_mask		/* flip the sign bit */
	strb	r3, [r0, #mval_b_exp]
	ldr	r3, [r1, #mval_l_m0]
	str	r3, [r0, #mval_l_m0]
	ldr	r3, [r1, #mval_l_m1]
	str	r3, [r0, #mval_l_m1]
done:
	mov	sp, fp
	pop	{fp, pc}

	.end
