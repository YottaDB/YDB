#################################################################
#								#
# Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2018 Stephen L Johnson. All rights reserved.	#
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

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.text
	.extern	forchkhandler
/*
 * This is the M profiling version which calls different routine(s) for M profiling purposes.
 */
ENTRY op_mprofforchk1
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	CHKSTKALIGN				/* Verify stack alignment */
	mov	x0, x30
	bl	forchkhandler
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
