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

/* opp_newvar.s */

/*
 * void op_newvar(uint4 arg1)
 */

	.title	opp_newvar.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	opp_newvar

	.data
	.extern	frame_pointer

	.text
	.extern	op_newvar

ENTRY opp_newvar
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_newvar
	getframe
	bx	lr

	.end
