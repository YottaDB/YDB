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

/* op_forchk1.s */

/* 
	op_forchk1 - dummy routine called at start of FOR-statement

	During normal execution, this routine would be called at the beginning
	of a For-statement. However, when it is desired to set a break at that
	location, the entry in the xfer table pointing to op_forchk1 would be
	altered to point to the desired alternative routine.
*/

	.title	op_forchk1.s
	.sbttl	op_forchk1

	.include "linkage.si"

	.text

/*
 * This routine just provides an interception point potential. No work happens here so no need to
 * check stack alignment. If ever a call is added then this routine should take care to align the stack
 * to 8 bytes and add a CHKSTKALIGN macro.
 */
ENTRY op_forchk1
	bx	lr

	.end
