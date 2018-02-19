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

/* opp_indtext.s */

/*
 * void op_indtext(mval *lab, mint offset, mval *rtn, mval *dst)
 */

	.title	opp_indtext.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	opp_indtext

	.data
	.extern	frame_pointer

	.text
	.extern	op_indtext

ENTRY opp_indtext
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indtext
	getframe
	bx	lr

	.end
