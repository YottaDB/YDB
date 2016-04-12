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
	.extern	gvcmz_neterr
	.extern	async_action
	.extern	outofband_clear

ENTRY	op_startintrrpt
	putframe
	subq	$8, REG_SP			# Allocate save area and align stack to 16 bytes
	CHKSTKALIGN				# Verify stack alignment
	cmpb	$0, neterr_pending(REG_IP)
	je	l1
	call	outofband_clear
	movq	$0, REG64_ARG0
	call	gvcmz_neterr
l1:
	movl	$1, REG32_ARG0
	call	async_action			# Normally does not return but just in case..
	addq	$16, REG_SP			# Remove alignment stack bump & burn return addr
	getframe				# Load regs for possible new frame and push return addr
	ret
