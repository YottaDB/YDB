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
	.title	op_equnul.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_equnul
#	PAGE	+
	.DATA
.extern	undef_inhibit

	.text
.extern	underr

# PUBLIC	op_equnul
ENTRY op_equnul
	enter $0, $0   # Align the stack to 16 bytes
	mv_if_notdefined REG64_RET0, l3
	testw	$mval_m_str,mval_w_mvtype(REG64_RET0)
	je	l2
	cmpl	$0,mval_l_strlen(REG64_RET0)
	jne	l2
l1:	movl	$1,REG32_RET0
	cmpl	$0,REG32_RET0
	leave
	ret

l2:	movl	$0,REG32_RET0
	cmpl	$0,REG32_RET0
	leave
	ret

l3:	cmpb	$0,undef_inhibit(REG_IP)	# not defined
	jne	l1			# if undef_inhibit, then all undefined
					# values are equal to null string
	movq	REG64_RET0,REG64_ARG0
	movb    $0,REG8_ACCUM             # variable length argumentt
	call	underr
	leave
	ret
# op_equnul ENDP

# END
