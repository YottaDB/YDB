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
	.extern	_neterr_pending

	.text
	.extern	_gtm_fetch
	.extern	_gvcmz_neterr
	.extern	_outofband_clear
	.extern	_async_action

ENTRY	_op_fetchintrrpt
	movq	_frame_pointer(%rip), %r11
	popq	msf_mpc_off(%r11)		# Save return addr in M frame, also aligns stack to 16 bytes
	CHKSTKALIGN					# Verify stack alignment
	movq    %r15, msf_ctxt_off(%r11)
	movb    $0, %al             		# Variable length argument
	call	_gtm_fetch
	cmpb	$0, _neterr_pending(%rip)
	je	l1
	call	_outofband_clear
	movq 	$0, %rdi
	call	_gvcmz_neterr
l1:
	movl	$1, %edi
	call	_async_action
	movq	_frame_pointer(%rip), %r11
	pushq	msf_mpc_off(%r11)		# Push return address for current frame back on stack
	ret
