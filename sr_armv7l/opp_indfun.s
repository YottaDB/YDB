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

/* opp_indfun.s */

/*
 * void op_indfun(mval *v, mint argcode, mval *dst)
 */

	.title	opp_indfun.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	opp_indfun

	.data
	.extern frame_pointer

	.text
	.extern	op_indfun

ENTRY opp_indfun
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indfun
	getframe
	bx	lr

	.end
