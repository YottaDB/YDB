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

	.text
	.extern	s2n
#
# Routine to force the input source mval to a number if it is not already so.
#
#	REG64_RET1 [r10] - source mval
#	REG64_RET0 [rax] - destination mval
#

save_ret0	= 0
save_ret1	= 8
FRAME_SIZE	= 24					# This frame size gives us a 16 byte aligned stack

ENTRY	op_forcenum
	subq	$FRAME_SIZE, REG_SP			# Allocate save area and align stack
	CHKSTKALIGN					# Verify stack alignment
	movq	REG64_RET0, save_ret0(REG_SP)
	mv_force_defined REG64_RET1, l00
	movq	REG64_RET1, save_ret1(REG_SP)
	mv_force_num REG64_RET1, l10
	movq 	save_ret1(REG_SP), REG64_RET1
	movq	save_ret0(REG_SP), REG64_RET0
	testw	$mval_m_str, mval_w_mvtype(REG64_RET1)
	jz	l20
	testw	$mval_m_num_approx, mval_w_mvtype(REG64_RET1)
	jz	l40
l20:
	testw	$mval_m_int_without_nm, mval_w_mvtype(REG64_RET1)
	jz	l30
	movw	$mval_m_int, mval_w_mvtype(REG64_RET0)
	movl	mval_l_m1(REG64_RET1), REG32_ARG2
	movl	REG32_ARG2, mval_l_m1(REG64_RET0)
	jmp	done

l30:
	movw	$mval_m_nm, mval_w_mvtype(REG64_RET0)
	movb	mval_b_exp(REG64_RET1), REG8_ARG2
	movb	REG8_ARG2, mval_b_exp(REG64_RET0)
	#
	# Copy the only numeric part of Mval from [r10] to [rax].
	#
	movl	mval_l_m0(REG64_RET1), REG32_ARG2
	movl	REG32_ARG2, mval_l_m0(REG64_RET0)
	movl	mval_l_m1(REG64_RET1), REG32_ARG2
	movl	REG32_ARG2, mval_l_m1(REG64_RET0)
	jmp	done
l40:
	#
	# Copy the Mval from REG64_RET1 [r10] to REG64_RET0 [rax].
	#
	movq	REG64_RET0, REG64_ARG0
	movq	REG64_RET1, REG64_ARG1
	movl	$mval_qword_len, REG32_ARG3
	REP
	movsq
done:
	addq	$FRAME_SIZE, REG_SP			# Remove save area from C stack
	ret
