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

	.data
	.extern	_param_list

	.text
#
# Note since this routine makes no calls, stack alignment is not critical. If ever a call is added then this
# routine should take care to align the stack to 16 bytes and add a CHKSTKALIGN macro.
#
ENTRY	_ci_restart
	movq	_param_list(%rip), %rdx
	movl	8(%rdx), %ecx
	cmpl 	$0, %ecx				# if (argcnt > 0) {
	jle 	L0
	leaq	48(%rdx), %rdx
	movq    0(%rdx), %r9
	subl	$1, %ecx
	leaq	8(%rdx), %rsi
	leaq	8(%rsp), %rdi
	REP
	movsq						# }
L0:
	movq	_param_list(%rip), %rax
	movl	8(%rax), %r8d		# argcnt
	movl	40(%rax), %ecx		# mask
	movq	32(%rax), %rdx		# retaddr
	movq	24(%rax), %rsi		# labaddr
	movq	16(%rax), %rdi		# rtnaddr
	jmp	*(%rax)
	#
	# No return from transfer of control
	#
