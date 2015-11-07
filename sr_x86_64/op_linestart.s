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

	.data
	.extern	frame_pointer

	.text

	#
	# Routine to save the current return address and context in the current stack frame.
	#
	# Since this routine is a leaf routine (no calls), its stack frame alignment is not critical. If that changes,
	# this routine should do the necessary to keep the stack 16 byte aligned and use the CHKSTKALIGN macro to verify
	# it is so.
	#
ENTRY	op_linestart
	movq    frame_pointer(REG_IP), REG64_RET1	# -> M frame
        movq    (REG_SP), REG64_ACCUM			# Fetch return address to save
        movq    REG64_ACCUM, msf_mpc_off(REG64_RET1)	# Save incoming return address in frame_pointer->mpc
	movq    REG_PV, msf_ctxt_off(REG64_RET1)	# Save ctxt in frame_pointer
	ret
