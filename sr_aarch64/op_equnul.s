#################################################################
#								#
# Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	#
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

/* op_equnul.s */

/*
 *	On entry:
 *		x0 - pointer to mval to compare against nul
 *	On exit:
 *		x0 - 1 if mval is null string, otherwise 0
 *		z flag is NOT set if null string, otherwise set
 */

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.data
	.extern	undef_inhibit

	.text
	.extern	underr

ENTRY op_equnul
	CHKSTKALIGN						/* Verify stack alignment */
	mv_if_notdefined x0, undefmval				/* See if a string */
	ldrh	w1, [x0, #mval_w_mvtype]
	tst	w1, #mval_m_str
	b.eq	notnullstr					/* If not a string, then not a null strin */
	ldr	w1, [x0, #mval_l_strlen]
	cbnz	w1, notnullstr					/* If not, not a null string */
nullstr:
	/*
	 * We have a null string. Return value not really used but set it for comparison purposes
	 */
	mov	w0, #1
	cmp	w0, wzr
	ret
notnullstr:
	/*
	 * We have either a non-null string or not a string at all.
	 */
	mov	w0, wzr
	cmp	w0, wzr
	ret
	/*
	 * Here when input mval not defined. If undef_inhibit set, make the assumption the value is the NULL string.
	 * Else, raise the undef error.
	 */
undefmval:	
	ldr	x1, =undef_inhibit				/* not defined */
	ldrb	w2, [x1]					/* if undef_inhibit, then all undefined */
	cbnz	w2, nullstr					/* values are equal to null string */

	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	bl	underr						/* really undef */
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret

