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

/* op_fnzextract.s */

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

/*
---------------------------------
	op_fnzextract (int last, int first, mval *src, mval *dest)
		w0 - last
		w1 - first
		x2 - *src
		x3 - *dest
---------------------------------
*/

last		=  -4
first		=  -8
src		= -16
dest		= -24
FRAME_SIZE	=  32

	.text

	.extern	n2s
	.extern underr

ENTRY op_fnzextract
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	sub	sp, sp, FRAME_SIZE
	str	w0, [x29, #last]
	str	w1, [x29, #first]
	str	x3, [x29, #dest]
	CHKSTKALIGN					/* Verify stack alignment */
	mv_force_defined x2
	str	x2, [x29, #src]
	mv_force_str x2
	ldr	x1, [x29, #src]
	ldr	w4, [x29, #first]
	cmp	w4, wzr
	b.gt	l10
	mov	w4, #1					/* If first < 1, then first = 1 */
l10:
	ldr	w2, [x29, #last]
	ldr	x0, [x29, #dest]
	mov	w15, #mval_m_str
	strh	w15, [x0, #mval_w_mvtype]		/* Always a string */
	ldr	w3, [x1, #mval_l_strlen]		/* w12 -- src str len */
	cmp	w3, w4					/* If left index > str len, then null result */
	b.lt	l25
	cmp	w3, w2					/* right index may be at most the len */
	b.ge	l20					/* of the source string */
	mov	w2, w3
l20:
	mov	w15, w2
	sub	w15, w15, w4				/* result len = end - start + 1 */
	adds	w15, w15, #1
	b.gt	l30					/* if len > 0, then continue */
l25:
	str	wzr, [x0, #mval_l_strlen]
	b	retlab
l30:
	str	w15, [x0, #mval_l_strlen]		/* dest str len */
	sub	w4, w4, #1				/* base = src addr + left index - 1 */
	ldr	x15, [x1, #mval_a_straddr]
	add	x15, x15, w4, sxtw
	str	x15, [x0, #mval_a_straddr]
retlab:
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
