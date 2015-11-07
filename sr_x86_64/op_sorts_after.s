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
#
# op_sorts_after(mval *mval1, *mval2)
#
# Call sorts_after() to determine whether mval1 comes after mval2
# in sorting order.  Use alternate local collation sequence if
# present.
#
# entry:
#   rax	mval *mval1
#   rdx	mval *mval2
#
# Sets condition flags and returns in eax:
#   1	mval1 > mval2
#   0	mval1 = mval2
#   -1	mval1 < mval2
#
	.text
	.extern	sorts_after

ENTRY	op_sorts_after
	subq	$8, REG_SP				# Bump stack for 16 byte alignment
	CHKSTKALIGN					# Verify stack alignment
	movq	REG64_RET1, REG64_ARG1
	movq	REG64_RET0, REG64_ARG0
	call	sorts_after
	addq	$8, REG_SP				# Remove stack alignment bump
	cmpl	$0, REG32_ACCUM				# Set flags according to result from
	ret
