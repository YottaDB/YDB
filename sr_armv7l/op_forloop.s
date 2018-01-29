#################################################################
#								#
# Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2017-2018 Stephen L Johnson.			#
# All rights reserved.						#
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
		r0 - ptr to index variable mval
		r1 - ptr to step mval
		r2 - ptr to terminator mval
		r3 - return address to continue looping
*/

	.title	op_forloop.s

	.include "linkage.si"
	.include "g_msf.si"
	.include "mval_def.si"
#	include "debug.si"

	.sbttl	op_forloop

	.data
	.extern	frame_pointer

	.text
	.extern	add_mvals
	.extern	numcmp
	.extern	s2n
	.extern underr

loop		= -16
term		= -12
step		=  -8
indx		=  -4
FRAME_SIZE	=  16

ENTRY op_forloop
	stmfd	sp!, {r6, fp}
	mov	fp, sp
	sub	sp, #FRAME_SIZE				/* Stack remains 8 byte aligned */
	CHKSTKALIGN					/* Verify stack alignment */

	ldr	r12, [r5]
	str	lr, [r12, #msf_mpc_off]			/* Save loop termination return address in M stack frame */
	/*
	 * Save the arguments on stack
	 */
 	str	r0, [fp, #indx]
	str	r1, [fp, #step]
	str	r2, [fp, #term]
	str	r3, [fp, #loop]

	mov	r1, r0
	mv_force_defined_strict r1
	mv_force_num r1
	ldr	r1, [fp, #indx]
	ldr	r0, [fp, #step]
	ldrh	r4, [r1, #mval_w_mvtype]
	ldrh	r12, [r0, #mval_w_mvtype]
	and	r4, r12
	tst	r4, #mval_m_int_without_nm
	beq	L66
	ldr	r4, [r1, #mval_l_m1]
	ldr	r12, [r0, #mval_l_m1]
	add	r4, r12
	ldr	r12, =MANT_HI
	cmp	r4, r12
	bge	L68
	rsb	r12, r12, #0
	cmp	r4, r12
	ble	L67
	mov	r12, #mval_m_int
	strh	r12, [r1, #mval_w_mvtype]
	str	r4, [r1, #mval_l_m1]
	b	L63
L67:
	mov	r12, #mval_esign_mask
	strb	r12, [r1, #mval_b_exp]			/* Set sign bit */
	rsb	r4, r4, #0
	b	L69
L68:
	mov	r12, #0
	strb	r12, [r1, #mval_b_exp]			/* Clear sign bit */
L69:
	mov	r12, #mval_m_nm
	strh	r12, [r1, #mval_w_mvtype]
	ldrb	r12, [r1, #mval_b_exp]
	orr	r12, #69
	strb	r12, [r1, #mval_b_exp]
.ifdef __armv7l__
	movw	r6, #0x6667				/* Divide r4 by 10 - result in r6 */
	movt	r6, #0x6666
.else	/* __armv6l__ */
	ldr	r6, =0x66666667				/* magic constant for divide by 10 */
.endif
	smull	r2, r6, r4, r6
	asr	r2, r6, #2
	asr	r6, r4, #31
	sub	r6, r2, r6				/* End of division */
	str	r6, [r1, #mval_l_m1]
	mov	r12, #10
	mul	r6, r12, r6
	sub	r4, r6, r4
	ldr	r12, =MANT_LO
	mul	r4, r12, r4
	str	r4, [r1, #mval_l_m0]
	b	L63
L66:
	mov	r3, r1
	mov	r2, #0
	mov	r1, r0
	mov	r0, r3
	bl	add_mvals
	ldr	r1, [fp, #indx]
L63:
	ldr	r0, [fp, #step]
	ldrh	r12, [r0, #mval_w_mvtype]
	tst	r12, #mval_m_int_without_nm
	bne	a
	ldrb	r12, [r0, #mval_b_exp]
	sxtb	r12, r12
	cmp	r12, #0
	blt	b
	b	a2
a:
	ldr	r12, [r0, #mval_l_m1]
	cmp	r12, #0
	blt	b
a2:
	ldr	r0, [fp, #term]
	b	e
b:
	mov	r0, r1			/* if step negative, reverse compare */
	ldr	r1, [fp, #term]
e:
	/*
	 * Compare indx and term
	 */
	ldrh	r4, [r1, #mval_w_mvtype]
	ldrh	r12, [r0, #mval_w_mvtype]
	and	r4, r12
	tst	r4, #2
	beq	ccmp
	ldr	r4, [r1, #mval_l_m1]
	ldr	r12, [r0, #mval_l_m1]
	subs	r4, r4, r12
	b	tcmp
ccmp:
	mov	r3, r0
	mov	r0, r1
	mov	r1, r3
	bl	numcmp
	cmp	r0, #0
tcmp:
	ble	newiter
	ldr	r1, [fp, #indx]
	ldr	r0, [fp, #step]
	ldrh	r4, [r1, #mval_w_mvtype]
	ldrh	r12, [r0, #mval_w_mvtype]
	and	r4, r12
	tst	r4, #mval_m_int_without_nm
	beq	l66
	ldr	r4, [r1, #mval_l_m1]
	ldr	r12, [r0, #mval_l_m1]
	sub	r4, r4, r12
	ldr	r12, =MANT_HI
	cmp	r4, r12
	bge	l68
	rsb	r12, r12, #0
	cmp	r4, r12
	ble	l67
	mov	r12, #mval_m_int
	strh	r12, [r1, #mval_w_mvtype]
	str	r4, [r1, #mval_l_m1]
	b	done
l67:
	mov	r12, #mval_esign_mask
	strb	r12, [r1, #mval_b_exp]			/* Set sign bit */
	rsb	r4, r4, #0
	b	l69
l68:
	mov	r12, #0
	strb	r12, [r1, #mval_b_exp]			/* Clear sign bit */
l69:
	mov	r12, #mval_m_nm
	strh	r12, [r1, #mval_w_mvtype]
	ldrb	r12, [r1, #mval_b_exp]
	orr	r12, #69
	strb	r12, [r1, #mval_b_exp]
.ifdef __armv7l__
	movw	r6, #0x6667				/* Divide r4 by 10 - result in r6 */
	movt	r6, #0x6666
.else	/* __armv6l__ */
	ldr	r6, =0x66666667				/* magic constant for divide by 10 */
.endif
	smull	r2, r6, r4, r6
	asr	r2, r6, #2
	asr	r6, r4, #31
	sub	r6, r2, r6				/* End of division */
	str	r6, [r1, #mval_l_m1]
	mov	r12, #10
	mul	r6, r12, r6
	sub	r4, r6, r4
	ldr	r12, =MANT_LO
	mul	r4, r12, r4
	str	r4, [r1, #mval_l_m0]
	b	done
l66:
	mov	r3, r1
	mov	r2, #1
	mov	r1, r0
	mov	r0, r3					/* First and fourth args are the same */
	bl	add_mvals
done:
	mov	sp, fp
	ldmfd   sp!, {r6, fp}
	ldr	r12, [r5]
	ldr	lr, [r12, #msf_mpc_off]
	bx	lr
newiter:
	/*
	 * Return to loop return address for another iteration
	 */
	mov	sp, fp
	ldr	lr, [fp, #loop]
	ldmfd   sp!, {r6, fp}
	bx	lr

	.end
