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

/* opp_zgoto.s */

/*
 * void op_zgoto(mval *rtn_name, mval *lbl_name, int offset, int level)
 */

	.title	opp_zgoto.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	opp_zgoto

	.data
	.extern	frame_pointer

	.text
	.extern	op_zgoto

ENTRY opp_zgoto
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_zgoto
	getframe
	bx	lr

	.end
