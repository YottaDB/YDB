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

	.data
	.extern	restart_pc
	.extern restart_ctxt
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
	movq	REG64_ACCUM, restart_pc(REG_IP)
	movq	frame_pointer(REG_IP), REG64_ACCUM
	movq	msf_ctxt_off(REG64_ACCUM), REG64_SCRATCH1
	movq	REG64_SCRATCH1, restart_ctxt(REG_IP)
	ret
# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
# This marking is not an issue in Linux but is in Windows Subsystem on Linux (WSL) which does not enable executable stack.
.section        .note.GNU-stack,"",@progbits
