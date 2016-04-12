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

	.include "linkage.si"
	.include "g_msf.si"
	.include "mval_def.si"
	.include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	numcmp
	.extern	s2n

arg0_save	= 0
arg1_save	= 8
arg2_save	= 16
FRAME_SIZE	= 32					# 32 bytes of save area

ENTRY	op_forinit
	movq	frame_pointer(REG_IP), REG64_SCRATCH1
	movq    REG_PV, msf_ctxt_off(REG64_SCRATCH1)
	popq	msf_mpc_off(REG64_SCRATCH1)		# Save return address (and 16 byte align the stack)
	subq	$FRAME_SIZE, REG_SP			# Allocate save area
	CHKSTKALIGN					# Verify stack alignment
	movq	REG64_ARG2, arg2_save(REG_SP)		# Save args to avoid getting modified across function calls
	movq	REG64_ARG0, arg0_save(REG_SP)
	movq	REG64_ARG1, REG64_ACCUM			# Copy 2nd argument (REG64_ARG1)
	mv_force_defined REG64_ACCUM, t1
	movq	REG64_ACCUM, arg1_save(REG_SP)		# Save (possibly modified) 2nd argument (REG64_ARG1)
	mv_force_num REG64_ACCUM, t2
	movq	arg1_save(REG_SP), REG64_ACCUM		# Restore 2nd argument (REG64_ARG1)
	cmpl	$0, mval_l_m1(REG64_ACCUM)
	js	l2
	mv_if_int REG64_ACCUM, l1
	testb 	$mval_esign_mask, mval_b_exp(REG64_ACCUM)
	jne	l2
l1:
	movq	arg0_save(REG_SP), REG64_ARG0		# Compare first with third
	movq	arg2_save(REG_SP), REG64_ARG1
	jmp	comp
l2:
	movq	arg2_save(REG_SP), REG64_ARG0		# Compare third with first
	movq	arg0_save(REG_SP), REG64_ARG1
comp:
	call	numcmp
	addq	$FRAME_SIZE, REG_SP			# Unwind stack frame savearea
	movq	frame_pointer(REG_IP), REG64_SCRATCH1
	pushq	msf_mpc_off(REG64_SCRATCH1)		# Push return addr back on stack
	cmpl	$0, REG32_RET0				# Set condition code for caller
	ret
