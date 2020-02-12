#################################################################
#								#
# Copyright (c) 2007-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2017-2020 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
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
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	numcmp
	.extern	mval2num
	.extern	s2n

arg0_save	= 0
arg1_save	= 8
arg2_save	= 16
FRAME_SIZE	= 32					# 32 bytes of save area

ENTRY	op_forinit
	movq	frame_pointer(%rip), %r11
	movq    %r15, msf_ctxt_off(%r11)
	popq	msf_mpc_off(%r11)			# Save return address (and 16 byte align the stack)
	subq	$FRAME_SIZE, %rsp			# Allocate save area
	CHKSTKALIGN					# Verify stack alignment
	movq	%rdx, arg2_save(%rsp)			# Save args to avoid getting modified across function calls
	movq	%rdi, arg0_save(%rsp)
	movq	%rsi, %rdi				# Copy 2nd argument (%rsi) as 1st argument for mval2num call
	call	mval2num				# Convert to a numeric. Issue ZYSQLNULLNOTVALID error if needed.
	cmpl	$0, mval_l_m1(%rax)			# %rax holds potentially updated `mval *` value from mval2num() call
	js	l2
	mv_if_int %rax, l1
	testb 	$mval_esign_mask, mval_b_exp(%rax)
	jne	l2
l1:
	movq	arg0_save(%rsp), %rdi			# Compare first with third
	movq	arg2_save(%rsp), %rsi
	jmp	comp
l2:
	movq	arg2_save(%rsp), %rdi			# Compare third with first
	movq	arg0_save(%rsp), %rsi
comp:
	call	numcmp
	addq	$FRAME_SIZE, %rsp			# Unwind stack frame savearea
	movq	frame_pointer(%rip), %r11
	pushq	msf_mpc_off(%r11)			# Push return addr back on stack
	cmpl	$0, %eax				# Set condition code for caller
	ret
# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
# This marking is not an issue in Linux but is in Windows Subsystem on Linux (WSL) which does not enable executable stack.
.section        .note.GNU-stack,"",@progbits
