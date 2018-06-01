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
	cmp	w2, #5
	b.le	cont
	/*
	 * We have 1 or more local variable names to save/restore so we need some aligned space on the stack for
	 * parameters that don't fit in the 4 parm registers. No var name parameters can fit in parm registers though
	 * a one var was initially in a parm reg but is now to be shifted to the stack since this code adds a parm. All
	 * other parms must reside on the stack starting at the lowest address. So for example, if we need 7 slots, we
	 * must allocate 8 slots to keep the stack aligned but the 7 slots used must be those with the lowest address for
	 * this to work correctly.
	 */
	ADJ_STACK_ALIGN_ODD_ARGS w2
	mov	w13, w2						/* Total number of args */
	sub	w13, w13, #1					/* But one arg is in r3 so it doesn't get copied */
	tst	w13, #1						/* See if count was odd or even */
	beq	loop						/* Original count was not even so no extra word used to align sp */
	sub	x27, x27, #8					/* Skip over the word that was used to align sp */
loop:
	ldr	x12, [x27, #-8]!
	sub	sp, sp, #8
	str	x12, [sp]
	subs	w13, w13, #1
	b.ne	loop
	CHKSTKALIGN						/* Verify stack alignment */
cont:
	mov	w3, w2
	mov	x2, x1
	mov	x1, x0
	mov	x0, xzr						/* arg0: NOT an implicit op_tstart() call */
	bl	op_tstart

	mov	x29, x27					/* Restore entry value of frame pointer */
	mov	sp, x29
	getframe
	ret
