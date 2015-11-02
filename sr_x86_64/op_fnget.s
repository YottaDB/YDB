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
	.title	op_fnget.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_fnget
#	PAGE	+
# ------------------------------------
# op_fnget.s
#
# Mumps $Get function
# ------------------------------------

#	edx->REG64_RET1 - src. mval
#	eax->REG64_RET0 - dest. mval

	.text
# PUBLIC	op_fnget
ENTRY op_fnget
	cmpq	$0,REG64_RET1
	je	l5			# if arg = 0, set type and len
	mv_if_notdefined REG64_RET1, l5
	movl	$mval_byte_len,REG32_ARG3	#Size of mval
	movq	REG64_RET1,REG64_ARG1		#Set source
	movq	REG64_RET0,REG64_ARG0		#Set destination
	REP					#Repeat until count is zero
	movsb
	andw	$~mval_m_aliascont, mval_w_mvtype(REG64_RET0)	# Don't propagate alias container flag
	ret
l5:	movw	$mval_m_str,mval_w_mvtype(REG64_RET0)
	movl	$0,mval_l_strlen(REG64_RET0)
	ret
# op_fnget ENDP

# END
