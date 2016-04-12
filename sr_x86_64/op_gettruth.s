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

	.data
	.extern	_dollar_truth
	.extern	_literal_one
	.extern	_literal_zero

	.text
	#
	# Routine to fetch mval representing value of $TEST (formerly $TRUTH).
	#
	# Note this routine is a leaf routine so does no stack-alignment or checking. If that changes, this routine
	# needs to use CHKSTKALIGN macro and make sure stack is 16 byte aligned.
	#
ENTRY	_op_gettruth
	cmpl	$0, _dollar_truth(%rip)
	jne	l1
	leaq	_literal_zero(%rip), %rsi
	jmp	doit

l1:
	leaq	_literal_one(%rip), %rsi
doit:
	#
	# Copy/return literal_zero or _literal_one mval to caller
	#
	movq	%r10, %rdi
	movl	$mval_byte_len, %ecx
	REP
	movsb
	ret
