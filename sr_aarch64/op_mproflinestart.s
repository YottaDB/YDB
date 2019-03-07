#################################################################
#								#
# Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	#
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

/* op_mproflinestart.s */

/*
	op_mproflinestart - establish start of line in GT.M MUMPS stack frame
*/

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	pcurrpos
/*
 * This is the M profiling version which calls different routine(s) for M profiling purposes.
 */
ENTRY op_mproflinestart
	mov	x28, x30					/* Save link pointer in register, not on stack */
	CHKSTKALIGN						/* Verify stack alignment */
	ldr	x9, [x19]
	str	x30, [x9, #msf_mpc_off]
	str	x24, [x9, #msf_ctxt_off]			/* save ctxt into frame pointer */
	bl	pcurrpos
	br	x28
