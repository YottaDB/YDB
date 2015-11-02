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
	.title	op_follow.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_follow
#	PAGE	+

sav_rax	=	-8
sav_rdx	=	-16

	.text
.extern	memvcmp
.extern	n2s

# PUBLIC	op_follow
ENTRY op_follow
	enter   $16, $0
        movq    REG64_RET1,sav_rdx(REG_FRAME_POINTER)
	mv_force_defined REG64_RET0, l1
	movq	REG64_RET0,sav_rax(REG_FRAME_POINTER)
	mv_force_str REG64_RET0, l2
        movq    sav_rdx(REG_FRAME_POINTER),REG64_RET1
	mv_force_defined REG64_RET1, l3
	movq    REG64_RET1,sav_rdx(REG_FRAME_POINTER)
	mv_force_str REG64_RET1, l4
	movq	sav_rax(REG_FRAME_POINTER),REG64_RET0
        movq    sav_rdx(REG_FRAME_POINTER),REG64_RET1
	movl	mval_l_strlen(REG64_RET1),REG32_ARG3
	movq 	mval_a_straddr(REG64_RET1),REG64_ARG2
	movl    mval_l_strlen(REG64_RET0),REG32_ARG1
	movq	mval_a_straddr(REG64_RET0),REG64_ARG0
	call	memvcmp
	cmpl	$0,REG32_RET0
	leave
	ret
# op_follow ENDP

# END
