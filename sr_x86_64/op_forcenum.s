#################################################################
#								#
#	Copyright 2007, 2008 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_forcenum.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_forcenum
#	PAGE	+
	.text
.extern	s2n

#	r10 - source mval
#	rax - destination mval

# PUBLIC	op_forcenum
ENTRY op_forcenum
	pushq	REG64_RET0
	mv_force_defined REG64_RET1, l00
	pushq	REG64_RET1
	mv_force_num REG64_RET1, l10
	popq 	REG64_RET1
	popq	REG64_RET0

	testw	$mval_m_str,mval_w_mvtype(REG64_RET1)
	je	l20
	testw	$mval_m_num_approx,mval_w_mvtype(REG64_RET1)
	je	l40
l20:	testw	$mval_m_int_without_nm,mval_w_mvtype(REG64_RET1)
	je	l30
	movw	$mval_m_int,mval_w_mvtype(REG64_RET0)
	movl	mval_l_m1(REG64_RET1),REG32_ARG2
	movl	REG32_ARG2,mval_l_m1(REG64_RET0)
	ret

l30:	pushq	REG_XFER_TABLE
	movw	$mval_m_nm,mval_w_mvtype(REG64_RET0)
	movb	mval_b_exp(REG64_RET1),REG8_ARG2
	movb	REG8_ARG2,mval_b_exp(REG64_RET0)

#	Copy the only numeric part of Mval from [edx] to [eax].

	movl	mval_l_m0(REG64_RET1),REG32_ARG2
	movl	REG32_ARG2,mval_l_m0(REG64_RET0)
	movl	mval_l_m1(REG64_RET1),REG32_ARG2
	movl	REG32_ARG2,mval_l_m1(REG64_RET0)
	popq	REG_XFER_TABLE
	ret

l40:
#	Copy the Mval from [edx] to [eax].

	pushq	REG64_ARG0
	pushq	REG64_ARG1
	movq	REG64_RET0,REG64_ARG0
	movq	REG64_RET1,REG64_ARG1
	movl	$mval_byte_len,REG32_ARG3
	REP
	movsb
	popq	REG64_ARG1
	popq	REG64_ARG0
	ret
# op_forcenum ENDP

# END
