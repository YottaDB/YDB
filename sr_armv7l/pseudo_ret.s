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

/* pseudo_ret */
/*	PSEUDO_RET calls opp_ret (which doesn't return).  It executes in a
	GT.M MUMPS stack frame and is, in fact, normally entered via a
	getframe/ret instruction sequence. */

	.title	pseudo_ret.s

	.include "linkage.si"
#	include "debug.si"

	.sbttl	pseudo_ret

ENTRY pseudo_ret
	CHKSTKALIGN					/* Verify stack alignment */
	bl	opp_ret

	.end
