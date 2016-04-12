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

	.include "linkage.si"
	.include "g_msf.si"
	.include "debug.si"

	.data
	.extern	_frame_pointer

	.text
	.extern	_gtm_fetch

	#
	# This routine does local variable fetch for all variables on a given line of code, or alternatively, all
	# variables in a routine depending on arguments from generated code we pass-thru.
	#
	# Since this routine pops its return address off the stack, the stack becomes 16 byte aligned. Verify that.
	#
ENTRY	_op_linefetch
	movq	_frame_pointer(%rip), %rax
	popq	msf_mpc_off(%rax)		# Save incoming return PC in frame_pointer->mpc
	movq	%r15, msf_ctxt_off(%rax)	# Save linkage pointer
	CHKSTKALIGN					# Verify stack alignment
	movb    $0, %al				# No variable length arguments
	call	_gtm_fetch
	movq	_frame_pointer(%rip), %rax
	pushq	msf_mpc_off(%rax)		# Push return address back on stack for return
	ret
