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

/* mint2mval.s */
/*	Convert to int to mval
 *		r0 - pointer to mval to receive value
 *		r1 - int to convert
 */

	.title	mint2mval.s
	.sbttl	mint2mval

	.include "linkage.si"
#	include "debug.si"

	.text

ENTRY mint2mval
	push	{r4, lr}				/* r4 is to maintain 8 byte stack alignment */
	CHKSTKALIGN					/* Verify stack alignment */
	bl	i2mval
	pop	{r4, pc}

	.end


