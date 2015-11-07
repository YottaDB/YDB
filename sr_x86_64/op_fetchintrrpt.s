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
	.extern	neterr_pending

	.text
	.extern	gtm_fetch
	.extern	gvcmz_neterr
	.extern	outofband_clear
	.extern	async_action

ENTRY	op_fetchintrrpt
	movq	frame_pointer(REG_IP), REG64_SCRATCH1
	popq	msf_mpc_off(REG64_SCRATCH1)		# Save return addr in M frame, also aligns stack to 16 bytes
	CHKSTKALIGN					# Verify stack alignment
	movq    REG_PV, msf_ctxt_off(REG64_SCRATCH1)
	movb    $0, REG8_ACCUM             		# Variable length argument
	call	gtm_fetch
	cmpb	$0, neterr_pending(REG_IP)
	je	l1
	call	outofband_clear
	movq 	$0, REG64_ARG0
	call	gvcmz_neterr
l1:
	movl	$1, REG32_ARG0
	call	async_action
	movq	frame_pointer(REG_IP), REG64_SCRATCH1
	pushq	msf_mpc_off(REG64_SCRATCH1)		# Push return address for current frame back on stack
	ret
