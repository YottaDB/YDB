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

/* op_forloop.s */

/*
	Called with the register contents:
		x0 - ptr to index variable mval
		x1 - ptr to step mval
		x2 - ptr to terminator mval
		x3 - return address to continue looping
*/

	.include "linkage.si"
	.include "g_msf.si"
	.include "mval_def.si"
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	add_mvals
	.extern	numcmp
	.extern	s2n

loop		= -32
term		= -24
step		= -16
indx		=  -8
FRAME_SIZE	=  48

ENTRY op_forloop
	stp	x9, x29, [sp, #-16]!				/* x9 is to maintain 16 byte alignment */
	mov	x29, sp
	sub	sp, sp, #FRAME_SIZE				/* Stack remains 16 byte aligned */
	CHKSTKALIGN						/* Verify stack alignment */

	ldr	x27, [x19]
	str	x30, [x27, #msf_mpc_off]			/* Save loop termination return address in M stack frame */
	/*
	 * Save the arguments on stack
	 */
 	str	x0, [x29, #indx]
	str	x1, [x29, #step]
	str	x2, [x29, #term]
	str	x3, [x29, #loop]

	mov	x1, x0
	mv_force_defined_strict x1
	mv_force_num x1
	ldr	x1, [x29, #indx]
	ldr	x0, [x29, #step]
	ldrh	w13, [x1, #mval_w_mvtype]
	ldrh	w15, [x0, #mval_w_mvtype]
	and	w13, w13, w15
	tst	w13, #mval_m_int_without_nm
	b.eq	L66
	ldr	w13, [x1, #mval_l_m1]
	ldr	w15, [x0, #mval_l_m1]
	add	w13, w13, w15
	ldr	w15, =MANT_HI
	cmp	w13, w15
	b.ge	L68
	sub	w15, wzr, w15
	cmp	w13, w15
	b.le	L67
	mov	w15, #mval_m_int
	strh	w15, [x1, #mval_w_mvtype]
	str	w13, [x1, #mval_l_m1]
	b	L63
L67:
	mov	w15, #mval_esign_mask
	strb	w15, [x1, #mval_b_exp]				/* Set sign bit */
	sub	w13, wzr, w13
	b	L69
L68:
	mov	w15, wzr
	strb	w15, [x1, #mval_b_exp]				/* Clear sign bit */
L69:
	mov	w15, #mval_m_nm
	strh	w15, [x1, #mval_w_mvtype]
	ldrb	w15, [x1, #mval_b_exp]
	mov	w14, #69					/* Set exponent field */
	orr	w15, w15, w14
	strb	w15, [x1, #mval_b_exp]
	mov	w15, #10					/* Divide w13 by 10, result to w14 */
	udiv	w14, w13, w15
	str	w14, [x1, #mval_l_m1]
	mul	w14, w15, w14
	sub	w13, w14, w13
	ldr	w15, =MANT_LO
	mul	w13, w15, w13
	str	w13, [x1, #mval_l_m0]
	b	L63
L66:
	mov	x3, x1
	mov	w2, wzr
	mov	x1, x0
	mov	x0, x3
	bl	add_mvals
	ldr	x1, [x29, #indx]
L63:
	ldr	x0, [x29, #step]
	ldrh	w15, [x0, #mval_w_mvtype]
	tst	w15, #mval_m_int_without_nm
	b.ne	a
	ldrb	w15, [x0, #mval_b_exp]
	sxtb	w15, w15
	cmp	w15, wzr
	b.lt	b
	b	a2
a:
	ldr	w15, [x0, #mval_l_m1]
	cmp	w15, wzr
	b.lt	b
a2:
	ldr	x0, [x29, #term]
	b	e
b:
	mov	x0, x1						/* if step negative, reverse compare */
	ldr	x1, [x29, #term]
e:
	/*
	 * Compare indx and term
	 */
	ldrh	w13, [x1, #mval_w_mvtype]
	ldrh	w15, [x0, #mval_w_mvtype]
	and	w13, w13, w15
	tst	w13, #2
	b.eq	ccmp
	ldr	w13, [x1, #mval_l_m1]
	ldr	w15, [x0, #mval_l_m1]
	subs	w13, w13, w15
	b	tcmp
ccmp:
	mov	x12, x0
	mov	x0, x1
	mov	x1, x12
	bl	numcmp
	cmp	w0, wzr
tcmp:
	b.le	newiter
	ldr	x1, [x29, #indx]
	ldr	x0, [x29, #step]
	ldrh	w13, [x1, #mval_w_mvtype]
	ldrh	w15, [x0, #mval_w_mvtype]
	and	w13, w13, w15
	tst	w13, #mval_m_int_without_nm
	b.eq	l66
	ldr	w13, [x1, #mval_l_m1]
	ldr	w15, [x0, #mval_l_m1]
	sub	w13, w13, w15
	ldr	w15, =MANT_HI
	cmp	w13, w15
	b.ge	l68
	sub	w15, wzr, w15
	cmp	w13, w15
	b.le	l67
	mov	w15, #mval_m_int
	strh	w15, [x1, #mval_w_mvtype]
	str	w13, [x1, #mval_l_m1]
	b	done
l67:
	mov	w15, #mval_esign_mask
	strb	w15, [x1, #mval_b_exp]				/* Set sign bit */
	sub	w13, wzr, w13
	b	l69
l68:
	strb	wzr, [x1, #mval_b_exp]				/* Clear sign bit */
l69:
	mov	w15, #mval_m_nm
	strh	w15, [x1, #mval_w_mvtype]
	ldrb	w15, [x1, #mval_b_exp]
	mov	w14, #69					/* Set exponent field */
	orr	w15, w15, w14
	strb	w15, [x1, #mval_b_exp]
	mov	w15, #10					/* Divide w13 by 10, result to w14 */
	udiv	w14, w13, w15
	str	w14, [x1, #mval_l_m1]
	mul	w14, w15, w14
	sub	w13, w14, w13
	ldr	w15, =MANT_LO
	mul	w13, w15, w13
	str	w13, [x1, #mval_l_m0]
	b	done
l66:
	mov	x3, x1
	mov	w2, #1
	mov	x1, x0
	mov	x0, x3						/* First and fourth args are the same */
	bl	add_mvals
done:
	mov	sp, x29
	ldp	x9, x29, [sp], #16				/* x9 was to maintain 16 byte alignment */
	ldr	x27, [x19]
	ldr	x30, [x27, #msf_mpc_off]
	ret
newiter:
	/*
	 * Return to loop return address for another iteration
	 */
	mov	sp, x29
	ldr	x30, [x29, #loop]
	ldp	x9, x29, [sp], #16				/* x9 was to maintain 16 byte alignment */
	ret
