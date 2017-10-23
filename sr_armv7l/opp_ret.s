#################################################################
#								#
# Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

/* opp_ret.s */

/*
 * void op_unwind(void)
 */

	.title	opp_ret.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_ret

	.data
.extern	frame_pointer

	.text
.extern	op_unwind


ENTRY opp_ret
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_unwind
	getframe
	bx	lr

.end
