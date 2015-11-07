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
	.include "mval_def.si"
	.include "debug.si"

sav_rax	=	-8
sav_rdx	=	-16

	.text
	.extern	memvcmp
	.extern	n2s

ENTRY	op_follow
	pushq	%rbp				# Save %rbp (aka REG_FRAME_POINTER) - aligns stack to 16 bytes
	movq	REG_SP, %rbp
	subq	$16, REG_SP			# Get 16 byte save area
	CHKSTKALIGN				# Verify stack alignment
        movq    REG64_RET1, sav_rdx(%rbp)
	mv_force_defined REG64_RET0, l1
	movq	REG64_RET0, sav_rax(%rbp)
	mv_force_str REG64_RET0, l2
        movq    sav_rdx(%rbp), REG64_RET1
	mv_force_defined REG64_RET1, l3
	movq    REG64_RET1, sav_rdx(%rbp)
	mv_force_str REG64_RET1, l4
	movq	sav_rax(%rbp), REG64_RET0
        movq    sav_rdx(%rbp), REG64_RET1
	movl	mval_l_strlen(REG64_RET1), REG32_ARG3
	movq 	mval_a_straddr(REG64_RET1), REG64_ARG2
	movl    mval_l_strlen(REG64_RET0), REG32_ARG1
	movq	mval_a_straddr(REG64_RET0), REG64_ARG0
	call	memvcmp
	addq	$16, REG_SP
	popq	%rbp
	cmpl	$0, REG32_RET0			# Set condition code for use by caller
	ret
