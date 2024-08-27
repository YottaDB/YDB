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

# op_mval2bool.s
#	Convert mval to bool.
# args:
#	See mval2bool.c for input args details
#

	.title	mval2bool.s

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.sbttl	op_mval2bool

	.text
	.extern mval2bool

ENTRY op_mval2bool
	push	{r6, lr}
	CHKSTKALIGN		/* Verify stack alignment */
	bl	mval2bool	/* Call C function `mval2bool` with the same parameters that we were passed in with.
				 * This does the bulk of the needed $ZYSQLNULL processing for boolean expression evaluation.
				 * The `bool_result` return value would be placed in `r0`.
				 */
	cmp	r0, #0
	pop	{r6, pc}

	.end

# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
.section        .note.GNU-stack,"",@progbits
