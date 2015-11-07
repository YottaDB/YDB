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
	.extern	copy_stack_frame

#
# op_call - Sets up a local routine call (does not leave routine)
#
# Argument:
#	REG64_ARG0 - Value from OCNT_REF triple that contains the byte offset from the return address
#		     where the local call should actually return to.
#
ENTRY	op_calll
ENTRY	op_callw
ENTRY	op_callb
	movq	(REG_SP), REG64_ACCUM			# Save return addr in reg
	subq	$8, REG_SP				# Bump stack for 16 byte alignment
	CHKSTKALIGN					# Verify stack alignment
	movq	frame_pointer(REG_IP), REG64_SCRATCH1
	movq	REG64_ACCUM, msf_mpc_off(REG64_SCRATCH1) # Save return addr in M frame
	addq	REG64_ARG0, msf_mpc_off(REG64_SCRATCH1)	# Add in return offset
	call	copy_stack_frame			# Copy current stack frame for local call
	addq	$8, REG_SP				# Remove stack alignment bump
	ret
