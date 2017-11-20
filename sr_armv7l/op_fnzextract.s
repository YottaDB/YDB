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

/* op_fnzextract.s */

	.title	op_fnzextract.s

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.sbttl	op_fnzextract

/*
---------------------------------
	op_fnzextract (int last, int first, mval *src, mval *dest)
		r0 - last
		r1 - first
		r2 - *src
		r3 - *dest
---------------------------------
*/

last	=	 -4
first	=	 -8
src	=	-12
dest	=	-16

	.text
	.extern	n2s
	.extern underr

ENTRY op_fnzextract
	stmfd   sp!, {r4, fp, r12, lr}
	CHKSTKALIGN				/* Verify stack alignment */
	mov	fp, sp
	sub	sp, sp, #16
	str	r0, [fp, #last]
	str	r1, [fp, #first]
	str	r3, [fp, #dest]
	mv_force_defined r2
	str	r2, [fp, #src]
	mv_force_str r2
	ldr	r1, [fp, #src]
	ldr	r4, [fp, #first]
	cmp	r4, #0
	bgt	l10
	mov	r4, #1					/* if first < 1, then first = 1 */
l10:	ldr	r2, [fp, #last]
	ldr	r0, [fp, #dest]
	mov	r12, #mval_m_str
	strh	r12, [r0, #mval_w_mvtype]		/* always a string */
	ldr	r3, [r1, #mval_l_strlen]		/* r3 - src str len */
	cmp	r3, r4					/* if left index > str len,
							/* then null result */
	blt	l25
	cmp	r3, r2					/* right index may be at most the len */
	bge	l20					/* of the source string */
	mov	r2, r3
l20:	mov	r12, r2
	sub	r12, r12, r4				/* result len = end - start + 1 */
	adds	r12, #1
	bgt	l30					/* if len > 0, then continue */
l25:	mov	r12, #0
	str	r12, [r0, #mval_l_strlen]
	b	retlab

l30:	str	r12, [r0, #mval_l_strlen]		/* dest str len */
	sub	r4, #1					/* base = src addr + left index - 1 */
	ldr	r12, [r1, #mval_a_straddr]
	add	r12, r4
	str	r12, [r0, #mval_a_straddr]

retlab:	mov	sp, fp
	ldmfd   sp!, {r4, fp, r12, pc}


	.end
