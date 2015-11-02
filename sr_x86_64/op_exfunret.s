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
	.title	op_exfunret.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_exfunret
#	PAGE	+
	.DATA
.extern	ERR_QUITARGREQD

	.text
.extern	rts_error

# PUBLIC	op_exfunret
ENTRY op_exfunret
	movw	mval_w_mvtype(REG64_ARG0),REG16_ACCUM
	andw	$~mval_m_retarg,mval_w_mvtype(REG64_ARG0)
	andw	$mval_m_retarg,REG16_ACCUM
	jne	l1
	movl	ERR_QUITARGREQD(REG_IP),REG32_ARG1
	movl	$1,REG32_ARG0
	movb    $0,REG8_ACCUM             # variable length argumentt
	call	rts_error
l1:	ret
# op_exfunret ENDP

# END
