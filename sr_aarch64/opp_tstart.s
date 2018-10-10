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

/* opp_tstart.s */

/*
 * Wrapper for op_tstart that rebuffers the arguments and adds an arg to the front of the list to so op_tstart
 * knows whether it was called from generated code or from C code since it handles TP restarts differently in
 * those cases. This routine also saves/reloads the stackframe and related pointers because on an indirect call
 * op_tstart() may shift the stack frame due to where it needs to put the TPHOST mv_stent.
 *
 * Parameters:
 *    arg0:      (int) SERIAL flag
 *    arg1:      (mval *) transaction id
 *    arg2:	 -2	preserve all variables
 *		 -1	not restartable
 *		 >= 0	count of local vars to be saved/restored
 *    arg3-arg7: (mval *) list of (arg2) mvals (continuing on stack if needed) containing the NAMES of variables
 *               to be saved and restored on a TP restart.
 */

	.include "linkage.si"
	.include "g_msf.si"
	.include "stack.si"
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	op_tstart

ENTRY opp_tstart
	putframe
	mov	x27, x29					/* save entry value of frame pointer */
	mov	x29, sp
	CHKSTKALIGN
	cmp	w2, wzr
	b.le	no_arg
	cmp	w2, #1
	b.eq	one_arg
	cmp	w2, #2
	b.eq	two_arg
	cmp	w2, #3
	b.eq	three_arg
	cmp	w2, #4
	b.eq	four_arg
	/*
	 * We have 1 or more local variable names to save/restore so we need some aligned space on the stack for
	 * parameters that don't fit in the 8 parm registers. Four var name parameters can fit in parm registers though
	 * one var was initially in a parm reg but is now to be shifted to the stack since this code adds a parm. All
	 * other parms must reside on the stack starting at the lowest address. So for example, if we need 7 slots, we
	 * must allocate 8 slots to keep the stack aligned but the 7 slots used must be those with the lowest address for
	 * this to work correctly.
	 */
	ADJ_STACK_ALIGN_ODD_ARGS w2
	mov	x28, sp						/* str xnn, [sp, #-8]! only works for 16 byte aligns, so use x28.
								 * At the end x28 will be 16 byte aligned, so sp will be OK */
	cmp	w2, #5
	b.eq	five_arg					/* Param in x7 doesn't get copied, but is pushed directly */
	mov	w13, w2						/* Total number of args */
	sub	w13, w13, #5					/* Five args were in registers so they don't get copied */
	tst	w13, #1						/* See if count was odd or even */
	beq	loop						/* Original count was not even so no extra word used to align sp */
	sub	x27, x27, #8					/* Skip over the word that was used to align sp */
loop:
	ldr	x12, [x27, #-8]!
	str	x12, [x28, #-8]!
	subs	w13, w13, #1
	b.ne	loop
five_arg:
	str	x7, [x28, #-8]!
	mov	sp, x28						/* Put sp back where it should be */
	CHKSTKALIGN						/* Verify stack alignment */
four_arg:
	mov	x7, x6
three_arg:
	mov	x6, x5
two_arg:
	mov	x5, x4
one_arg:
	mov	x4, x3
no_arg:
	mov	w3, w2
	mov	x2, x1
	mov	x1, x0
	mov	x0, xzr						/* arg0: NOT an implicit op_tstart() call */
	bl	op_tstart

	mov	x29, x27					/* Restore entry value of frame pointer */
	mov	sp, x29
	getframe
	ret
