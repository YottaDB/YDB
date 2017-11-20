#################################################################
#								#
# Copyright (c) 2007-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	copy_stack_frame_sp

#
# op_mprofcall - Sets up a local routine call (does not leave routine)
#
# This is the M profiling version which calls different routine(s) for M profiling purposes.
#
# Argument:
#	REG64_ARG0 - Value from OCNT_REF triple that contains the byte offset from the return address
#		     where the local call should actually return to.
#
ENTRY	op_mprofcalll
ENTRY	op_mprofcallw
ENTRY	op_mprofcallb
	movq	(REG_SP),REG64_ACCUM			# Save return addr in reg
	subq	$8, REG_SP				# Bump stack for 16 byte alignment
	CHKSTKALIGN					# Verify stack alignment
	movq	frame_pointer(REG_IP), REG64_SCRATCH1
	movq	REG64_ACCUM, msf_mpc_off(REG64_SCRATCH1) # Save return addr in M frame
	addq	REG64_ARG0, msf_mpc_off(REG64_SCRATCH1)	# Add in return offset
	call	copy_stack_frame_sp			# Copy current stack frame for local call
	addq	$8, REG_SP				# Remove stack alignment bump
	ret
# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
# This marking is not an issue in Linux but is in Windows Subsystem on Linux (WSL) which does not enable executable stack.
.section        .note.GNU-stack,"",@progbits
