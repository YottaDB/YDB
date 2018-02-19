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
 *    arg3-...:  (mval *) list of (arg2) mvals (continuing on stack if needed) containing the NAMES of variables
 *               to be saved and restored on a TP restart.
 */

	.title	opp_tstart.s

	.include "linkage.si"
	.include "g_msf.si"
	.include "stack.si"
#	include "debug.si"

	.sbttl	opp_tstart

	.data
	.extern	frame_pointer

	.text
	.extern	op_tstart

orig_fp		=	-4
FRAME_SAVE_SIZE	=	 8

ENTRY opp_tstart
	putframe
	mov	r12, fp					/* save entry value of frame pointer */
	mov	fp, sp
	sub	sp, #FRAME_SAVE_SIZE
	CHKSTKALIGN
	str	r12, [fp, #orig_fp]
	cmp	r2, #0
	ble	no_arg
	/*
	 * We have 1 or more local variable names to save/restore so we need some aligned space on the stack for
	 * parameters that don't fit in the 4 parm registers. No var name parameters can fit in parm registers though
	 * a one var was initially in a parm reg but is now to be shifted to the stack since this code adds a parm. All
	 * other parms must reside on the stack starting at the lowest address. So for example, if we need 7 slots, we
	 * must allocate 8 slots to keep the stack aligned but the 7 slots used must be those with the lowest address for
	 * this to work correctly.
	 */
	ADJ_STACK_ALIGN_ODD_ARGS r2
	cmp	r2, #1
	beq	one_arg
	mov	r4, r2					/* Total number of args */
	sub	r4, #1					/* But one arg is in r3 so it doesn't get copied */
	tst	r4, #1					/* See if count was odd or even */
	beq	loop					/* Original count was not even so no extra word used to align sp */
	sub	r12, #4					/* Skip over the word that was used to align sp */
loop:
	ldr	r5, [r12, #-4]!				/* Temporarily borrow r5 as we go down the stack */
	push	{r5}
	subs	r4, #1
	bne	loop
	ldr	r5, =frame_pointer			/* Restore r5 to its correct value */
one_arg:
	push	{r3}
	CHKSTKALIGN					/* Verify stack alignment */
no_arg:
	mov	r3, r2
	mov	r2, r1
	mov	r1, r0
	mov	r0, #0					/* arg0: NOT an implicit op_tstart() call */
	bl	op_tstart

	ldr	fp, [fp, #orig_fp]
	mov	sp, fp
	getframe
	bx	lr

	.end
