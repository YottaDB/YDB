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
 *		r0	mval *mval1
 *		r1	mval *mval2
 *
 *   Sets condition flags and returns in eax:
 *          1     mval1 > mval2
 *          0     mval1 = mval2
 *         -1     mval1 < mval2
 *
 */
	.title	op_sorts_after.s

	.include "linkage.si"
#	include "debug.si"

	.sbttl	op_sorts_after

	.text
	.extern	sorts_after

ENTRY op_sorts_after
	push	{r4, lr}				/* r4 is to maintain 8 byte stack alignment */
	CHKSTKALIGN					/* Verify stack alignment */
	bl	sorts_after
	cmp	r0, #0					/* Set flags according to result from sorts_after */
	pop	{r4, pc}

	.end
