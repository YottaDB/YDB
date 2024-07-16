#################################################################
#								#
# Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# opp_equnul_retbool.s
#	Convert mval to bool.
# args:
#	See op_equnul_retbool.c for input args details
#

	.title	opp_equnul_retbool.s

	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.sbttl	opp_equnul_retbool

	.text
	.extern op_equnul_retbool

ENTRY opp_equnul_retbool
	push	{r6, lr}
	CHKSTKALIGN		/* Verify stack alignment */
	bl	op_equnul_retbool
				/* Call C function `op_equnul_retbool` with the same parameters that we were passed in with.
				 * This does the bulk of the needed processing.
				 * The `bool_result` return value would be placed in `r0`.
				 */
	cmp	r0, #0
	pop	{r6, pc}

	.end
