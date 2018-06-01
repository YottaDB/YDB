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

/* op_retarg.s */
/*
 *	x0 - pointer to mval being returned
 *	w1 - True/False for alias return
 */

	.include "linkage.si"
	.include "g_msf.si"
	.include "stack.si"
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	unw_retarg
 
ENTRY op_retarg
	CHKSTKALIGN					/* Verify stack alignment */
	bl	unw_retarg
	getframe
	ret
