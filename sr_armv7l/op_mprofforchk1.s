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

/* op_mprofforchk1.s */
/*
	Called with arguments
		lr - call return address
*/

	.title	op_mprofforchk1.s

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.sbttl	op_mprofforchk1

	.data

	.text
	.extern	forchkhandler

/*
 * This is the M profiling version which calls different routine(s) for M profiling purposes.
 */
ENTRY op_mprofforchk1
	push	{r4, lr}			/* r4 is to maintain 8 byte stack alignment */
	CHKSTKALIGN				/* Verify stack alignment */
	mov	r0, lr
	bl	forchkhandler
	pop	{r4, pc}

	.end
