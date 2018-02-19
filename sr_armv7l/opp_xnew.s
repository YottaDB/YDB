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

/* opp_xnew.s */

/*
 * void op_xnew(unsigned int argcnt_arg, mval *s_arg, ...)
 *
 */

	.title	opp_xnew.s

	.include "linkage.si"
	.include "g_msf.si"
	.include "stack.si"
#	include "debug.si"

	.sbttl	opp_xnew

	.data
	.extern	frame_pointer

	.text
	.extern	op_xnew

ENTRY opp_xnew
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_xnew
	getframe
	bx	lr

	.end
