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

	.text
	.extern	do_patfixed
	.extern	do_pattern

ENTRY	op_pattern
	subq	$8, REG_SP				# Bump stack for 16 byte alignment
	CHKSTKALIGN					# Verify stack alignment
	movq	REG64_RET1, REG64_ARG1
	movq	REG64_RET0, REG64_ARG0
	movq	mval_a_straddr(REG64_RET1), REG64_ACCUM
	#
	# This is an array of unaligned ints. If the first word is zero, then call do_pattern
	# instead of do_patfixed. Only the low order byte is significant and so it is the only
	# one we need to test. We would do this in assembly because (1) we need the assmembly
	# routine anyway to set up the condition code the generated code needs and (2) it
	# saves an extra level of call linkage at the C level to do the decision here.
	#
	cmpb	$0, 0(REG64_ACCUM)			# Little endian compare of low order byte
	je	l1
	call	do_patfixed
	jmp	l2
l1:
	call	do_pattern
l2:
	addq	$8, REG_SP				# Remove stack alignment bump
	cmpl	$0, REG32_ACCUM				# Set condition code for generated code
	ret
