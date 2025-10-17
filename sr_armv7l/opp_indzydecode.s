#################################################################
#								#
# Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

/* opp_indzydecode.s */

/*
 * void op_indzydecode(mval *glvn_mv, mval *arg1_or_arg2)
 */

	.title	opp_indzydecode.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	opp_indzydecode

	.data
	.extern	frame_pointer

	.text
	.extern	op_indzydecode

ENTRY opp_indzydecode
	putframe
	CHKSTKALIGN					/* Verify stack alignment */
	bl	op_indzydecode
	getframe
	bx	lr

	.end

# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
.section        .note.GNU-stack,"",@progbits
