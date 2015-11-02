#################################################################
#								#
#	Copyright 2007 Fidelity Information Services, Inc	#
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

.include "g_msf.si"
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
	cmpl	$0,dollar_truth(REG_IP)
	jne	l1
	leaq	literal_zero(REG_IP),REG64_ARG1
	jmp	doit

l1:	leaq	literal_one(REG_IP),REG64_ARG1
doit:	movq	REG64_RET1,REG64_ARG0
	movl	$mval_byte_len,REG32_ARG3
	REP
	movsb
	ret
# op_gettruth ENDP

# END
