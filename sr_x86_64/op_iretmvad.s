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
	.extern	op_unwind

ENTRY	op_iretmvad
	putframe
	addq    $8, REG_SP		# Burn return PC and 16 byte align stack
	subq	$16, REG_SP		# Bump stack for 16 byte alignment and a save area
	CHKSTKALIGN			# Verify stack alignment
	movq	REG64_RET1, 0(REG_SP)	# Save input mval* value across call to op_unwind
	call	op_unwind
	movq	0(REG_SP), REG64_RET0	# Return input parameter via REG64_RET0
	addq	$16, REG_SP		# Unwind C frame save area
	getframe			# Pick up new stack frame regs & push return addr
	ret
