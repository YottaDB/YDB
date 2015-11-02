#################################################################
#								#
#	Copyright 2007, 2009 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_sto.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_sto
#	PAGE	+
	.DATA
.extern	literal_null
.extern	undef_inhibit

	.text
.extern	underr

# PUBLIC	op_sto
ENTRY op_sto
	enter $0, $0  # Align the stack to 16 bytes
	mv_if_notdefined REG64_RET1, b
a:	movl	$mval_byte_len,REG32_ARG3
	movq	REG64_RET1,REG64_ARG1
	movq	REG64_RET0,REG64_ARG0
	REP
	movsb
	andw	$~mval_m_aliascont, mval_w_mvtype(REG64_RET0)	# Don't propagate alias container flag
	leave
	ret

b:	cmpb	$0,undef_inhibit(REG_IP)
	je	clab
	leaq	literal_null(REG_IP),REG_RET1
	jmp	a

clab:	movq	REG_RET1, REG64_ARG0
	movb    $0,REG8_ACCUM             # variable length argument
	call	underr
	leave
	ret

# op_sto	ENDP

# END
