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

/* op_forinit.s */


	/* x0 - initial value */
	/* x1 - increment */
	/* x2 - final value */

	.include "linkage.si"
	.include "g_msf.si"
	.include "mval_def.si"
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	numcmp
	.extern	mval2num

arg2_save	= -24
arg1_save	= -16
arg0_save	=  -8
FRAME_SIZE	=  32					/* 32 bytes of save area */

ENTRY op_forinit
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	ldr	x9, [x19]
	str	x24, [x9, #msf_ctxt_off]
	sub	sp, sp, #32				/* Allocate save area */
	CHKSTKALIGN					/* Verify stack alignment */
	str	x0, [x29, #arg0_save]			/* Save args to avoid getting modified across function calls */
	str	x2, [x29, #arg2_save]
	mov	x0, x1					/* Copy 2nd argument as 1st argument for mval2num call */
	bl	mval2num				/* Convert to a numeric. Issue ZYSQLNULLNOTVALID error if needed */
	ldr	w15, [x0, #mval_l_m1]			/* x0 holds potentially updated `mval *` value from mval2num() call */
	cmp	w15, wzr
	b.mi	l2
	mv_if_int x0, l1
	ldrb	w15, [x0, #mval_b_exp]
	tst	w15, #mval_esign_mask
	b.ne	l2
l1:
	ldr	x0, [x29, #arg0_save]				/* Compare first with third */
	ldr	x1, [x29, #arg2_save]
	b	comp
l2:
	ldr	x0, [x29, #arg2_save]				/* Compare third with first */
	ldr	x1, [x29, #arg0_save]
comp:
	bl	numcmp
	cmp	w0, wzr						/* Set condition code for caller */
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
