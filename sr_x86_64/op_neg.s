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
	.title	op_neg.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_neg
#	PAGE	+
	.text
#
# op_neg ( mval *u, mval *v ) : u = -v
#
#	REG64_RET1 - source mval      = &v
#	REG64_RET0 - destination mval = &u

.extern	s2n
.extern underr

# PUBLIC	op_neg
ENTRY op_neg
	pushq	REG64_RET0
	mv_force_defined REG64_RET1, isdefined
	popq	REG64_RET0
	mv_if_number REG64_RET1, numer
	pushq	REG64_RET0
	pushq	REG64_RET1
	movq	REG64_RET1,REG64_ARG0
	call	s2n
	popq	REG64_RET1
	popq	REG64_RET0
numer:	mv_if_notint REG64_RET1, float
	movw	$mval_m_int,mval_w_mvtype(REG64_RET0)
	movl	mval_l_m1(REG64_RET1),REG32_RET1
	negl	REG32_RET1
	movl	REG32_RET1,mval_l_m1(REG64_RET0)
	ret

float:	movw	$mval_m_nm,mval_w_mvtype(REG64_RET0)
	movb	mval_b_exp(REG64_RET1),REG8_SCRATCH1
	xorb	$mval_esign_mask,REG8_SCRATCH1		# flip the sign bit
	movb	REG8_SCRATCH1,mval_b_exp(REG64_RET0)
	movl	mval_l_m0(REG64_RET1),REG32_SCRATCH1
	movl	REG32_SCRATCH1,mval_l_m0(REG64_RET0)
	movl	mval_l_m1(REG64_RET1),REG32_SCRATCH1
	movl	REG32_SCRATCH1,mval_l_m1(REG64_RET0)
	ret
# op_neg	ENDP

#END
