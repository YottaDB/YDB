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

/* op_equnul.s */

/*
 *	On entry:
 *		r0 - pointer to mval to compare against nul
 *	On exit:
 *		r0 - 1 if mval is null string, otherwise 0
 *		z flag is NOT set if null string, otherwise set
 */

	.title	op_equnul.s

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.sbttl	op_equnul

	.data
	.extern	undef_inhibit

	.text
	.extern	underr


ENTRY op_equnul
	CHKSTKALIGN				/* Verify stack alignment */
	mv_if_notdefined r0, l3
	ldrh	r1, [r0, #mval_w_mvtype]
	tst	r1, #mval_m_str
	beq	l2
	ldr	r1, [r0, #mval_l_strlen]
	cmp	r1, #0
	bne	l2
l1:	mov	r0, #1
	cmp	r0, #0			/* reset z flag */
	bx	lr

l2:	mov	r0, #0
	cmp	r0, #0			/* set z flag */
	bx	lr

l3:	ldr	r1, =undef_inhibit	/* not defined */
	ldrb	r1, [r1]		/* if undef_inhibit, then all undefined */
	cmp	r1, #0			/* values are equal to null string */
	bne	l1

	push	{r4, lr}		/* r4 is to maintain 8 byte stack alignment */
	bl	underr			/* really undef */
	pop	{r4, pc}

	.end

