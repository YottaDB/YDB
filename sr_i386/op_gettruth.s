#################################################################
#								#
# Copyright (c) 2001-2026 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_gettruth.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_gettruth
#	PAGE	+
	.DATA
.extern	dollar_truth
.extern	literal_one
.extern	literal_zero

	.text
# PUBLIC	op_gettruth
ENTRY op_gettruth
	pushl	%edi
	pushl	%esi

	cmpl	$0,dollar_truth
	jne	l1
	leal	literal_zero,%esi
	jmp	doit

l1:	leal	literal_one,%esi
doit:	movl	%edx,%edi
	movl	$umval_byte_len,%ecx
	REP
	movsb

	popl	%esi
	popl	%edi
	ret
# op_gettruth ENDP

# END
