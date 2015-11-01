#################################################################
#								#
#	Copyright 2001 Sanchez Computer Associates, Inc.	#
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
	popl	%eax
	popl	%edx
	pushl	%eax
	movb	mval_b_mvtype(%edx),%al
	andb	$~mval_m_retarg,mval_b_mvtype(%edx)
	andb	$mval_m_retarg,%al
	jne	l1
	pushl	ERR_QUITARGREQD
	pushl	$1
	call	rts_error
	addl	$8,%esp
l1:	ret
# op_exfunret ENDP

# END
