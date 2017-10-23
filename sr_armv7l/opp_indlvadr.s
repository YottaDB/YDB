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

/* opp_indlvadr.s */

/*
 * void	op_indlvadr(mval *target)
 */

	.title	opp_indlvadr.s

.include "linkage.si"
.include "g_msf.si"
.include "debug.si"

	.sbttl	opp_indlvadr

	.data
.extern	frame_pointer

	.text
.extern	op_indlvadr


ENTRY opp_indlvadr
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indlvadr
	getframe
	bx	lr


.end
