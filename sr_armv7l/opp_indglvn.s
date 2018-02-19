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

/* opp_indglvn.s */

/*
 * void	op_indglvn(mval *v,mval *dst)
 */

	.title	opp_indglvn.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	opp_indglvn

	.data
	.extern	frame_pointer

	.text
	.extern	op_indglvn

ENTRY opp_indglvn
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indglvn
	getframe
	bx	lr

	.end
