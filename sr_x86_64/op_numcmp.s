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
# op_numcmp calls numcmp to compare two mvals
#
# entry:
#   rax	- mval *u
#   rdx	- mval *v
#
# exit:
#   condition codes set according to value of numcmp(u, v)
#

	.text
	.extern	numcmp

ENTRY	op_numcmp
	subq	$8, REG_SP			# Bump stack for 16 byte alignment
	CHKSTKALIGN				# Verify stack alignment
	movq	REG64_RET1, REG64_ARG1
	movq	REG64_RET0, REG64_ARG0
	call	numcmp
	addq	$8, REG_SP			# Remove stack alignment bump
	cmpq	$0, REG64_ACCUM			# Set flags according to result from numcmp
	ret
