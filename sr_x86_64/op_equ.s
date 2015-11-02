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
	.title	op_equ.s
	.sbttl	op_equ

.include "g_msf.si"
.include "linkage.si"

#	.386
#	.MODEL	FLAT, C

	.text
.extern	is_equ

# PUBLIC	op_equ
ENTRY op_equ
	movq	REG_RET1, REG64_ARG1
	movq	REG_RET0, REG64_ARG0
	call	is_equ
	cmpl	$0,REG32_RET0
	ret
# op_equ	ENDP

# END
