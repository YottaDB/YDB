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
	.title	op_iretmvad.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_iretmvad
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	op_unwind

# PUBLIC	op_iretmvad
ENTRY op_iretmvad
	movq	REG64_RET1,REG64_ARG3
	putframe
	addq    $8,REG_SP              # burn return PC
	movq	REG64_ARG3,REG64_ARG2
	pushq	REG64_RET1
	call	op_unwind
	popq	REG64_RET0		# return input parameter
	getframe
	ret
# op_iretmvad ENDP

# END
