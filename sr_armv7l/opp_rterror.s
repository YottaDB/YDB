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

/* opp_rterror.s */

/*
 * void op_rterror(int4 sig, boolean_t subrtn)
 *
 *	On entry:
 *		r0 - integer value of signal
 *		r1 - True/False as to being called from an M subroutine
 */
	.title	opp_rterror.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	opp_rterror

	.data
	.extern	frame_pointer

	.text
	.extern	op_rterror

ENTRY opp_rterror
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_rterror
	getframe
	bx	lr

	.end
