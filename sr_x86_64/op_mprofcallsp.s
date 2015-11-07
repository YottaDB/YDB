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
	.extern	dollar_truth
	.extern	frame_pointer

	.text
	.extern	exfun_frame_push_dummy_frame
	.extern	push_tval

#
# op_mprofcallsp - Used to build a new stack level for argumentless DO (also saves $TEST)
#
# This is the M profiling version which calls different routine(s) for M profiling purposes.
#
# Argument:
#	REG64_ARG0 - Value from OCNT_REF triple that contains the byte offset from the return address
#		     to return to when the level pops.
#
ENTRY	op_mprofcallspl
ENTRY	op_mprofcallspw
ENTRY	op_mprofcallspb
	movq	(REG_SP), REG64_ACCUM			# Save return addr in reg
	subq	$8, REG_SP				# Bump stack for 16 byte alignment
	CHKSTKALIGN					# Verify stack alignment
	movq	frame_pointer(REG_IP), REG64_SCRATCH1
	movq	REG64_ACCUM, msf_mpc_off(REG64_SCRATCH1) # Save return addr in M frame
	addq	REG64_ARG0, msf_mpc_off(REG64_SCRATCH1) # Add in return offset
	call	exfun_frame_push_dummy_frame		# Copies stack frame and creates new temps
	movl	dollar_truth(REG_IP), REG32_ARG0
	call	push_tval
	movq	frame_pointer(REG_IP), REG_FRAME_POINTER
	movq	msf_temps_ptr_off(REG_FRAME_POINTER), REG_FRAME_TMP_PTR
	addq	$8, REG_SP				# Remove stack alignment bump
	ret
