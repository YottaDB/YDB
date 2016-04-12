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
	.extern	_gvcmz_neterr
	.extern	_async_action
	.extern	_outofband_clear

ENTRY	_op_startintrrpt
	putframe
	subq	$8, %rsp			# Allocate save area and align stack to 16 bytes
	CHKSTKALIGN				# Verify stack alignment
	cmpb	$0, _neterr_pending(%rip)
	je	l1
	call	_outofband_clear
	movq	$0, %rdi
	call	_gvcmz_neterr
l1:
	movl	$1, %edi
	call	_async_action
	addq	$16, %rsp			# Remove alignment stack bump & burn return addr
	getframe				# Load regs for possible new frame and push return addr
	ret
