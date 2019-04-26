#################################################################
#								#
# Copyright (c) 2007-2019 Fidelity National Information		#
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
	.extern frame_pointer

	.text
#
# Routine to save the address of the *start* of this call along with its context as the restart point should this
# process encounter a restart situation (return from $ZTRAP or outofband call typically).
#
# Since this is a leaf routine (makes no calls), the stack frame alignment is not important so is not adjusted
# or tested. Should that change, the alignment should be fixed and implement use of the CHKSTKALIGN macro made.
#
ENTRY op_restartpc
	movq	(REG_SP), REG64_ACCUM
	subq	$6, REG64_ACCUM 				# XFER call size is constant
	movq	frame_pointer(REG_IP), REG64_SCRATCH1
	movq	REG64_ACCUM, msf_restart_pc_off(REG64_SCRATCH1)
	movq	msf_ctxt_off(REG64_SCRATCH1), REG64_ACCUM
	movq	REG64_ACCUM, msf_restart_ctxt_off(REG64_SCRATCH1)
	ret
