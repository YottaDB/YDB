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

/* opp_indset.s */

/*
 * void	op_indset(mval *target, mval *value)
 */

	.title	opp_indset.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indset

	.data
.extern	frame_pointer

	.text
.extern	op_indset


ENTRY opp_indset
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indset
	getframe
	bx	lr

.end
