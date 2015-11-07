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

	.include "g_msf.si"
	.include "linkage.si"
	.include "debug.si"

	.text
	.extern	is_equ

ENTRY	op_equ
	subq	$8, REG_SP				# Bump stack for 16 byte alignment
	CHKSTKALIGN					# Verify stack alignment
	movq	REG_RET1, REG64_ARG1
	movq	REG_RET0, REG64_ARG0
	call	is_equ
	addq	$8, REG_SP				# Remove stack alignment bump
	cmpl	$0, REG32_RET0
	ret
