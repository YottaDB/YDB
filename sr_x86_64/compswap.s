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
	#
	# A(latch longword)	Arg0
	# comparison value	Arg1
	# replacement value	Arg2
	#
	# cmpxchg will compare REG32_RET0 i.e EAX with 1st arg so copy
	# comparison value to EAX (REG32_RET0).
	#
	# Note since this routine makes no calls, stack alignment is not critical. If ever a call is added then this
	# routine should take care to align the stack to 16 bytes and add a CHKSTKALIGN macro.
	#

	.text
ENTRY	compswap
	movl	REG32_ARG1, REG32_RET0
	lock
	cmpxchgl  REG32_ARG2, 0(REG64_ARG0)	# compare-and-swap
	jnz	fail
	movl	$1, REG32_RET0			# Return TRUE
	ret
fail:
	xor	REG32_RET0, REG32_RET0		# Return FALSE
	pause
	ret
