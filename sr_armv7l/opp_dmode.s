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

/* opp_dmode.s */

/*
 * void	op_dmode(void)
 */

	.title	opp_dmode.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_dmode

	.data
.extern	frame_pointer

	.text
.extern	op_dmode


ENTRY opp_dmode
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_dmode
	getframe
	bx	lr

.end
