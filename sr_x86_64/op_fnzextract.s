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

#
# op_fnzextract.s
#
# Mumps $Extract function
#
#	op_fnzextract (int last, int first, mval *src, mval *dest)
#

last		= -12
first		= -16
src		= -24
dest		= -32
FRAME_SIZE	= 32

	.text
	.extern	n2s

ENTRY	op_fnzextract
	pushq	%rbp					# Preserve caller's %rpb register (aka REG_STACK_FRAME)
	movq	REG_SP, %rbp				# Save current stack pointer to %rbp
	subq	$FRAME_SIZE, REG_SP			# Allocate save area for parameters
	CHKSTKALIGN					# Verify stack alignment
	movl	REG32_ARG0, last(%rbp)			# Save the parameters
	movl	REG32_ARG1, first(%rbp)
	movq	REG64_ARG3, dest(%rbp)
	mv_force_defined REG64_ARG2, l00
	movq	REG64_ARG2, src(%rbp)
	mv_force_str REG64_ARG2, l01
	movq	src(%rbp), REG64_ARG1
	movl	first(%rbp), REG32_ACCUM
	cmpl	$0, REG32_ACCUM
	jg	l10
	movl	$1, REG32_ACCUM				# If first < 1, then first = 1
l10:
	movl	last(%rbp), REG32_ARG2
	movq	dest(%rbp), REG64_ARG0
	movw	$mval_m_str, mval_w_mvtype(REG64_ARG0)
	movl	mval_l_strlen(REG64_ARG1), REG32_ARG3
	cmpl	REG32_ACCUM, REG32_ARG3			# If left index > str. len, then null result
	jl	l25
	cmpl	REG32_ARG2, REG32_ARG3			# Right index may be at most the len.
	jge	l20					# .. of the source string
	movl	REG32_ARG3, REG32_ARG2
l20:
	movl	REG32_ARG2, REG32_SCRATCH1
	subl	REG32_ACCUM, REG32_SCRATCH1		# Result len. = end - start + 1
	addl	$1, REG32_SCRATCH1
	jg	l30					# If len > 0, then continue
l25:
	movl	$0, mval_l_strlen(REG64_ARG0)
	jmp	retlab
l30:
	movl 	REG32_SCRATCH1, mval_l_strlen(REG64_ARG0)
	subl	$1, REG32_ACCUM				# Base = src.addr + left ind. - 1
	addq	mval_a_straddr(REG64_ARG1), REG64_ACCUM
	movq	REG64_ACCUM, mval_a_straddr(REG64_ARG0)
retlab:
	addq	$FRAME_SIZE, REG_SP			# Pull save area back off of stack
	popq	%rbp
	ret
