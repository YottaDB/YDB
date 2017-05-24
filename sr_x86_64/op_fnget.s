#################################################################
#								#
# Copyright (c) 2007-2016 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
	.include "debug.si"

#
# op_fnget.s
#
# Mumps $Get function
#
#
#	r10->REG64_RET1 - src. mval
#	rax->REG64_RET0 - dest. mval
#
# Note there is no stack padding for alignment and no check in this routine because it is a leaf routine
# so never calls anything else. The stack is unaligned by 8 bytes due to the return register but that is
# not an issue unless this routine calls something in the future in which case it needs changes to pad
# the stack for alignment and should then also use the CHKSTKALIGN macro to verify it.
#

	.text

ENTRY	op_fnget
	cmpq	$0, REG64_RET1
	je	l5				# Source mval does not exist
	mv_if_notdefined REG64_RET1, l5		# Branch if source mval is not defined
	movl	$mval_qword_len, REG32_ARG3	# Size of mval
	movq	REG64_RET1, REG64_ARG1		# Set source
	movq	REG64_RET0, REG64_ARG0		# Set destination
	REP					# Repeat until count is zero
	movsq
	andw	$~mval_m_aliascont, mval_w_mvtype(REG64_RET0)	# Don't propagate alias container flag
	ret

	#
	# Source mval either non-existent or undefined. Set return mval to null string and return it
	#
l5:
	movw	$mval_m_str, mval_w_mvtype(REG64_RET0)
	movl	$0, mval_l_strlen(REG64_RET0)
	ret
