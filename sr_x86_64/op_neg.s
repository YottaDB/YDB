#################################################################
#								#
# Copyright (c) 2007-2015 Fidelity National Information 	#
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
	.extern underr

save_ret1	= 8
save_ret0	= 0
FRAME_SIZE	= 24					# This size 16 byte aligns the stack

#
# op_neg ( mval *u, mval *v ) : u = -v
#
#	REG64_RET1 - source mval      = &v
#	REG64_RET0 - destination mval = &u
#
ENTRY	op_neg
	subq	$FRAME_SIZE, REG_SP			# Create save area and 16 byte align stack
	CHKSTKALIGN					# Verify stack alignment
	movq	REG64_RET0, save_ret0(REG_SP)		# Save dest mval addr across potential call
	mv_force_defined REG64_RET1, isdefined
	mv_if_number REG64_RET1, numer			# Branch if numeric
	movq	REG64_RET1, save_ret1(REG_SP)		# Save src mval (may not be original if noundef set)
	movq	REG64_RET1, REG64_ARG0			# Move src mval to parm reg for s2n()
	call	s2n
	movq	save_ret1(REG_SP), REG64_RET1		# Restore source mval addr
numer:
	movq	save_ret0(REG_SP), REG64_RET0		# Restore destination mval addr
	mv_if_notint REG64_RET1, float			# Branch if not an integer
	movw	$mval_m_int, mval_w_mvtype(REG64_RET0)
	movl	mval_l_m1(REG64_RET1), REG32_RET1
	negl	REG32_RET1
	movl	REG32_RET1, mval_l_m1(REG64_RET0)
	jmp	done
float:
	movw	$mval_m_nm, mval_w_mvtype(REG64_RET0)
	movb	mval_b_exp(REG64_RET1), REG8_SCRATCH1
	xorb	$mval_esign_mask, REG8_SCRATCH1		# Flip the sign bit
	movb	REG8_SCRATCH1, mval_b_exp(REG64_RET0)
	movl	mval_l_m0(REG64_RET1), REG32_SCRATCH1
	movl	REG32_SCRATCH1, mval_l_m0(REG64_RET0)
	movl	mval_l_m1(REG64_RET1), REG32_SCRATCH1
	movl	REG32_SCRATCH1, mval_l_m1(REG64_RET0)
done:
	addq	$FRAME_SIZE, REG_SP			# Remove save area from C stack
	ret
