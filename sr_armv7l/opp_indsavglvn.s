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

/* opp_indsavglvn.s */

/*
 * void op_indsavglvn(mval *target, uint4 slot, uint4 do_ref)
 */

	.title	opp_indsavglvn.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	opp_indsavglvn

	.data
	.extern	frame_pointer

	.text
	.extern	op_indsavglvn

ENTRY opp_indsavglvn
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indsavglvn
	getframe
	bx	lr

	.end
