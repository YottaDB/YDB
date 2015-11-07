#################################################################
#								#
# Copyright (c) 2007-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

	.include "g_msf.si"
	.include "linkage.si"

	.text
#
# Note since this routine makes no calls, stack alignment is not critical. If ever a call is added then this
# routine should take care to align the stack to 16 bytes and add a CHKSTKALIGN macro.
#
ENTRY	aswp
	movl	REG32_ARG1, REG32_RET0
	lock
	xchg	(REG64_ARG0),REG32_RET0		# Return original value
	ret
