#################################################################
#								#
# Copyright (c) 2007-2016 Fidelity National Information		#
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

	.data
	.extern	param_list

	.text
#
# Note since this routine makes no calls, stack alignment is not critical. If ever a call is added then this
# routine should take care to align the stack to 16 bytes and add a CHKSTKALIGN macro.
#
ENTRY	ci_restart
	movq	param_list(REG_IP), REG64_ARG2
	movl	8(REG64_ARG2), REG32_ARG3
	cmpl 	$0, REG32_ARG3				# if (argcnt > 0) {
	jle 	L0
	leaq	48(REG64_ARG2), REG64_ARG2
	movq    0(REG64_ARG2), REG64_ARG5
	subl	$1, REG32_ARG3
	leaq	8(REG64_ARG2), REG64_ARG1
	leaq	8(REG_SP), REG64_ARG0
	REP						#   copy argument registers
	movsq						# }
L0:
	movq	param_list(REG_IP), REG64_ACCUM
	movl	8(REG64_ACCUM), REG32_ARG4		# argcnt
	movl	40(REG64_ACCUM), REG32_ARG3		# mask
	movq	32(REG64_ACCUM), REG64_ARG2		# retaddr
	movq	24(REG64_ACCUM), REG64_ARG1		# labaddr
	movq	16(REG64_ACCUM), REG64_ARG0		# rtnaddr
	jmp	*(REG64_ACCUM)
	#
	# No return from transfer of control
	#
