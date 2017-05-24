#################################################################
#								#
# Copyright (c) 2007-2016 Fidelity National Information		#
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

	.data
	.extern	dollar_truth
	.extern	literal_one
	.extern	literal_zero

	.text
	#
	# Routine to fetch mval representing value of $TEST (formerly $TRUTH).
	#
	# Note this routine is a leaf routine so does no stack-alignment or checking. If that changes, this routine
	# needs to use CHKSTKALIGN macro and make sure stack is 16 byte aligned.
	#
ENTRY	op_gettruth
	cmpl	$0, dollar_truth(REG_IP)
	jne	l1
	leaq	literal_zero(REG_IP), REG64_ARG1
	jmp	doit

l1:
	leaq	literal_one(REG_IP), REG64_ARG1
doit:
	#
	# Copy/return literal_zero or literal_one mval to caller
	#
	movq	REG64_RET1, REG64_ARG0
	movl	$mval_qword_len, REG32_ARG3
	REP
	movsq
	ret
