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
	.title	op_contain.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_contain
#	PAGE	+
	.text
sav_rax	=	-8
sav_rdx	=	-16

.extern	matchc
.extern	n2s

# PUBLIC	op_contain
ENTRY op_contain
	enter	$16,$0
	movq	REG64_RET1,sav_rdx(REG_FRAME_POINTER)
	mv_force_defined REG64_RET0, l1
	movq	REG64_RET0,sav_rax(REG_FRAME_POINTER)
	mv_force_str	REG64_RET0, l2
	movq	sav_rdx(REG_FRAME_POINTER),REG64_RET1
	mv_force_defined REG64_RET1, l3
	movq    REG64_RET1,sav_rdx(REG_FRAME_POINTER)
	mv_force_str	REG64_RET1, l4
	subq	$8,REG_SP
	movq	REG_SP,REG64_ARG5					# 6th Argument
	movq	$1,(REG_SP)						# init arg to 1.
	subq	$8,REG_SP
	movq	REG_SP,REG64_ARG4					# 5th Argument
	movq	sav_rax(REG_FRAME_POINTER),REG64_RET0
	movq	sav_rdx(REG_FRAME_POINTER),REG64_RET1
	movq 	mval_a_straddr(REG64_RET0),REG64_ARG3			# 4th Argument
	movl	mval_l_strlen(REG64_RET0),REG32_ARG2			# 3rd Argument
	movq	mval_a_straddr(REG64_RET1),REG64_ARG1			# 2nd Argument
	movl	mval_l_strlen(REG64_RET1),REG32_ARG0
	call	matchc
	movl	(REG_SP), REG32_RET0    # The 5th argument is a pointer to a int. So read only 4 bytes
	addq    $1, REG_SP
	cmpl	$0,REG32_RET0
	leave
	ret
# op_contain ENDP

# END
