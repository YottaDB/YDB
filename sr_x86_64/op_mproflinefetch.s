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
	.extern	frame_pointer

	.text
	.extern	gtm_fetch
	.extern pcurrpos
	.extern	stack_leak_check

	#
	# This routine does local variable fetch for all variables on a given line of code, or alternatively, all
	# variables in a routine depending on arguments from generated code we pass-thru.
	#
	# This is the M profiling version which calls extra routines for M profiling purposes.
	#
	# Since this routine pops its return address off the stack, the stack becomes 16 byte aligned. Verify that.
	#
ENTRY	op_mproflinefetch
	movq	frame_pointer(REG_IP), REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)		# Save incoming return PC in frame_pointer->mpc
	movq	REG_PV, msf_ctxt_off(REG64_ACCUM)	# Save linkage pointer
	CHKSTKALIGN					# Verify stack alignment
	movb    $0, REG8_ACCUM				# No variable length arguments
	call	gtm_fetch
	call	pcurrpos
	call	stack_leak_check
	movq	frame_pointer(REG_IP), REG64_ACCUM
	pushq	msf_mpc_off(REG64_ACCUM)		# Push return address back on stack for return
	ret
