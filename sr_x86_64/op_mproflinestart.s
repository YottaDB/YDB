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
	.extern	pcurrpos

#
# This is the M profiling version which calls different routine(s) for M profiling purposes.
#
ENTRY	op_mproflinestart
	movq	frame_pointer(REG_IP), REG64_RET1
	movq    (REG_SP), REG64_ACCUM			# Save return address
	subq	$8, REG_SP				# Bump stack for 16 byte alignment
	CHKSTKALIGN					# Verify stack alignment
	movq	REG64_ACCUM, msf_mpc_off(REG64_RET1)	# Store return addr in M frame
	movq    REG_PV, msf_ctxt_off(REG64_RET1)	# Save ctxt into M frame
	call	pcurrpos
	addq	$8, REG_SP				# Remove stack alignment bump
	ret
