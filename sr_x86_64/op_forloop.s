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
#
#	Called with the stack contents:
#		call return
#		ptr to index mval
#		ptr to step mval
#		ptr to terminator mval
#		loop address
#
	.data
	.extern	frame_pointer
ten_dd:	.long	10

	.text
	.extern	add_mvals
	.extern	numcmp
	.extern	s2n

indx		= -8
step		= -16
term		= -24
loop		= -32
FRAME_SIZE	= 40					# Includes 8 bytes padding to 16 byte align stack

ENTRY	op_forloop
	movq	frame_pointer(REG_IP), REG64_SCRATCH1
	popq	msf_mpc_off(REG64_SCRATCH1)
	pushq	%rbp					# Save %rbp (aka GTM_FRAME_POINTER)
	movq	REG_SP, %rbp				# Copy of stack pointer
	subq	$FRAME_SIZE, REG_SP			# Create save area that leaves stack 16 byte aligned
	CHKSTKALIGN					# Verify stack alignment
	#
	# Save the arguments on stack
	#
        movq	REG64_ARG3, loop(%rbp)
        movq	REG64_ARG2, term(%rbp)
        movq	REG64_ARG1, step(%rbp)
        movq	REG64_ARG0, indx(%rbp)
	movq	REG64_ARG0, REG64_ARG1
	mv_force_defined_strict REG64_ARG1, l0		# Disregard NOUNDEF - errors if not defined
	mv_force_num REG64_ARG1, l1
	movq	indx(%rbp), REG64_ARG1
	movq	step(%rbp), REG64_ARG0
	movw	mval_w_mvtype(REG64_ARG1), REG16_ACCUM
	movw	mval_w_mvtype(REG64_ARG0), REG16_ARG2
	andw	REG16_ARG2, REG16_ACCUM
	testw	$mval_m_int_without_nm, REG16_ACCUM
	je	L66
	movl	mval_l_m1(REG64_ARG1), REG32_ACCUM
	addl	mval_l_m1(REG64_ARG0), REG32_ACCUM
	cmpl	$MANT_HI, REG32_ACCUM
	jge	L68
	cmpl	$-MANT_HI, REG32_ACCUM
	jle	L67
	movw	$mval_m_int, mval_w_mvtype(REG64_ARG1)
	movl	REG32_ACCUM, mval_l_m1(REG64_ARG1)
	jmp	L63
L67:
	movb	$mval_esign_mask, mval_b_exp(REG64_ARG1) # Set sign bit
	negl	REG32_ACCUM
	jmp	L69
L68:
	movb	$0, mval_b_exp(REG64_ARG1)		# Clear sign bit
L69:
	movw	$mval_m_nm, mval_w_mvtype(REG64_ARG1)
	orb	$69, mval_b_exp(REG64_ARG1)		# Set exponent field
	movl	REG32_ACCUM, REG32_SCRATCH1
	movl	$0, REG32_ARG2
	idivl	ten_dd(REG_IP), REG32_ACCUM
	movl	REG32_ACCUM, mval_l_m1(REG64_ARG1)
	imull	$10, REG32_ACCUM, REG32_ACCUM
	subl	REG32_ACCUM, REG32_SCRATCH1
	imull	$MANT_LO, REG32_SCRATCH1, REG32_SCRATCH1
	movl	REG32_SCRATCH1, mval_l_m0(REG64_ARG1)
	jmp	L63
L66:
	movq	REG64_ARG1, REG64_ARG3
	movl	$0, REG32_ARG2
	movq	REG64_ARG0, REG64_ARG1
	movq	REG64_ARG3, REG64_ARG0
	call	add_mvals
	movq	indx(%rbp), REG64_ARG1
L63:
	movq	step(%rbp), REG64_ARG0
	testw	$mval_m_int_without_nm, mval_w_mvtype(REG64_ARG0)
	jne	a
	cmpb	$0, mval_b_exp(REG64_ARG0)
	jl	b
	jmp	a2
a:
	cmpl	$0, mval_l_m1(REG64_ARG0)
	jl	b
a2:
	movq	term(%rbp), REG64_ARG0
	jmp	e
b:
	movq	REG64_ARG1, REG64_ARG0			# If step is negative, reverse compare
	movq	term(%rbp), REG64_ARG1
e:
	#
	# Compare indx and term
	#
	movw	mval_w_mvtype(REG64_ARG1), REG16_ACCUM
	movw	mval_w_mvtype(REG64_ARG0), REG16_ARG2
	andw	REG16_ARG2, REG16_ACCUM
	testw	$2, REG16_ACCUM
	je	ccmp
	movl	mval_l_m1(REG64_ARG1), REG32_ACCUM
	subl	mval_l_m1(REG64_ARG0), REG32_ACCUM
	jmp	tcmp
ccmp:
	xchgq	REG64_ARG0, REG64_ARG1
	call	numcmp
	cmpl	$0, REG32_ACCUM
tcmp:
	jle	newiter
	movq	indx(%rbp), REG64_ARG1
	movq	step(%rbp), REG64_ARG0
	movw	mval_w_mvtype(REG64_ARG1), REG16_ACCUM
	movw	mval_w_mvtype(REG64_ARG0), REG16_ARG2
	andw	REG16_ARG2, REG16_ACCUM
	testw	$mval_m_int_without_nm, REG16_ACCUM
	je	l66
	movl	mval_l_m1(REG64_ARG1), REG32_ACCUM
	subl	mval_l_m1(REG64_ARG0), REG32_ACCUM
	cmpl	$MANT_HI, REG32_ACCUM
	jge	l68
	cmpl	$-MANT_HI, REG32_ACCUM
	jle	l67
	movw	$mval_m_int, mval_w_mvtype(REG64_ARG1)
	movl	REG32_ACCUM, mval_l_m1(REG64_ARG1)
	jmp	done
l67:
	movb	$mval_esign_mask, mval_b_exp(REG64_ARG1) # Set sign bit
	negl	REG32_ACCUM
	jmp	l69
l68:
	movb	$0, mval_b_exp(REG64_ARG1)		# Clear sign bit
l69:
	movw	$mval_m_nm, mval_w_mvtype(REG64_ARG1)
	orb	$69, mval_b_exp(REG64_ARG1)
	movl	REG32_ACCUM, REG32_SCRATCH1
	movl	$0, REG32_ARG2				# Set edx to 0 before div
	idivl	ten_dd(REG_IP), REG32_ACCUM
	movl	REG32_ACCUM, mval_l_m1(REG64_ARG1)
	imull	$10, REG32_ACCUM, REG32_ACCUM
	subl	REG32_ACCUM, REG32_SCRATCH1
	imull	$MANT_LO, REG32_SCRATCH1, REG32_SCRATCH1
	movl	REG32_SCRATCH1, mval_l_m0(REG64_ARG1)
	jmp	done
l66:
	movq	REG64_ARG1, REG64_ARG3
	movl	$1, REG32_ARG2
	movq	REG64_ARG0, REG64_ARG1
	movq	REG64_ARG3, REG64_ARG0			# First and fourth args are same
	call	add_mvals
done:
	movq	%rbp, REG_SP				# Unwind save frame
	popq	%rbp					# Restore caller's %rbp
	movq	frame_pointer(REG_IP), REG64_SCRATCH1
	pushq	msf_mpc_off(REG64_SCRATCH1)		# End of loop - return to caller
	ret
newiter:
	#
	# Return to loop return address for another iteration
	#
	movq	loop(%rbp), REG64_SCRATCH1		# Save loop return address
	movq	%rbp, REG_SP				# Unwind save frame
	popq	%rbp					# Restore caller's %rbp
	pushq	REG64_SCRATCH1				# Set return address
	ret
