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

/* op_sorts_after.s */

/*
 * op_sorts_after.s 80386
 *
 * op_sorts_after(mval *mval1, *mval2)
 *     Call sorts_after() to determine whether mval1 comes after mval2
 *     in sorting order.  Use alternate local collation sequence if
 *     present.
 *
 *	entry:
 *		x0	mval *mval1
 *		x1	mval *mval2
 *
 *   Sets condition flags and returns in w0:
 *          1     mval1 > mval2
 *          0     mval1 = mval2
 *         -1     mval1 < mval2
 *
 */

	.include "linkage.si"
#	include "debug.si"

	.text
	.extern	sorts_after
 
ENTRY op_sorts_after
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN					/* Verify stack alignment */
	bl	sorts_after
	cmp	w0, wzr					/* Set flags according to result from sorts_after */
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
