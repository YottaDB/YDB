#################################################################
#								#
# Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries.	#
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

/* opp_indpat.s */

/*
 * void	op_indpat(mval *v, mval *dst)
 */

	.title	opp_indpat.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	opp_indpat

	.data
	.extern	frame_pointer

	.text
	.extern	op_indpat

ENTRY opp_indpat
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indpat
	getframe
	bx	lr

	.end

# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
.section        .note.GNU-stack,"",@progbits
