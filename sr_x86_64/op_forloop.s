#################################################################
#								#
#	Copyright 2007, 2012 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_forloop.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_forloop
#	PAGE	+
#	Called with the stack contents:
#		call return
#		ptr to index mval
#		ptr to step mval
#		ptr to terminator mval
#		loop address
	.DATA
.extern	frame_pointer

ten_dd:
.long	10

	.text
.extern	add_mvals
.extern	numcmp
.extern	s2n

loop	=	32
term	=	24
step	=	16
indx	=	8

# PUBLIC	op_forloop
ENTRY op_forloop
	movq	frame_pointer(REG_IP),REG64_SCRATCH1
	popq	msf_mpc_off(REG64_SCRATCH1)
	#Push the arguments on stack Argument registers are used in intermediate logic
        pushq	REG64_ARG3		#loop address. Return address
        pushq	REG64_ARG2
        pushq	REG64_ARG1
        pushq	REG64_ARG0
	enter	$0, $0			#rbp pushed on stack and then rbp = rsp
	pushq	REG_XFER_TABLE
	movq	indx(REG_FRAME_POINTER),REG64_ARG1
	mv_force_defined_strict REG64_ARG1, l0			# disregard NOUNDEF
	movq	REG64_ARG1, indx(REG_FRAME_POINTER)
	mv_force_num REG64_ARG1, l1
	movq	indx(REG_FRAME_POINTER),REG64_ARG1
	movq	step(REG_FRAME_POINTER),REG64_ARG0
	movw	mval_w_mvtype(REG64_ARG1),REG16_ACCUM
	movw	mval_w_mvtype(REG64_ARG0),REG16_ARG2
	andw	REG16_ARG2,REG16_ACCUM
	testw	$mval_m_int_without_nm,REG16_ACCUM
	je	L66
	movl	mval_l_m1(REG64_ARG1),REG32_ACCUM
	addl	mval_l_m1(REG64_ARG0),REG32_ACCUM
	cmpl	$MANT_HI,REG32_ACCUM
	jge	L68
	cmpl	$-MANT_HI,REG32_ACCUM
	jle	L67
	movw	$mval_m_int,mval_w_mvtype(REG64_ARG1)
	movl	REG32_ACCUM,mval_l_m1(REG64_ARG1)
	jmp	L63

L67:	movb	$mval_esign_mask,mval_b_exp(REG64_ARG1)	#set sign bit
	negl	REG32_ACCUM
	jmp	L69

L68:	movb	$0,mval_b_exp(REG64_ARG1)			# clear sign bit
L69:	movw	$mval_m_nm,mval_w_mvtype(REG64_ARG1)
	orb	$69,mval_b_exp(REG64_ARG1)			# set exponent field
	movl	REG32_ACCUM,REG32_SCRATCH1
	movl	$0,REG32_ARG2
	idivl	ten_dd(REG_IP),REG32_ACCUM
	movl	REG32_ACCUM,mval_l_m1(REG64_ARG1)
	imull	$10,REG32_ACCUM,REG32_ACCUM
	subl	REG32_ACCUM,REG32_SCRATCH1
	imull	$MANT_LO,REG32_SCRATCH1,REG32_SCRATCH1
	movl	REG32_SCRATCH1,mval_l_m0(REG64_ARG1)
	jmp	L63

L66:	movq	REG64_ARG1,REG64_ARG3
	movl	$0,REG32_ARG2
	movq	REG64_ARG0,REG64_ARG1
	movq	REG64_ARG3,REG64_ARG0
	call	add_mvals
	movq	indx(REG_FRAME_POINTER),REG64_ARG1
L63:	movq	step(REG_FRAME_POINTER),REG64_ARG0
	testw	$mval_m_int_without_nm,mval_w_mvtype(REG64_ARG0)
	jne	a
	cmpb	$0,mval_b_exp(REG64_ARG0)
	jl	b
	jmp	a2

a:	cmpl	$0,mval_l_m1(REG64_ARG0)
	jl	b
a2:	movq	term(REG_FRAME_POINTER),REG64_ARG0
	jmp	e

b:	movq	REG64_ARG1,REG64_ARG0		# if step is negative, reverse compare
	movq	term(REG_FRAME_POINTER),REG64_ARG1
e:	# compare indx and term
	movw	mval_w_mvtype(REG64_ARG1),REG16_ACCUM
	movw	mval_w_mvtype(REG64_ARG0),REG16_ARG2
	andw	REG16_ARG2,REG16_ACCUM
	testw	$2,REG16_ACCUM
	je	ccmp
	movl	mval_l_m1(REG64_ARG1),REG32_ACCUM
	subl	mval_l_m1(REG64_ARG0),REG32_ACCUM
	jmp	tcmp

ccmp:	xchgq	REG64_ARG0,REG64_ARG1
	call	numcmp
	cmpl	$0,REG32_ACCUM
tcmp:	jle	d
	movq	indx(REG_FRAME_POINTER),REG64_ARG1
	movq	step(REG_FRAME_POINTER),REG64_ARG0
	movw	mval_w_mvtype(REG64_ARG1),REG16_ACCUM
	movw	mval_w_mvtype(REG64_ARG0),REG16_ARG2
	andw	REG16_ARG2,REG16_ACCUM
	testw	$mval_m_int_without_nm,REG16_ACCUM
	je	l66
	movl	mval_l_m1(REG64_ARG1),REG32_ACCUM
	subl	mval_l_m1(REG64_ARG0),REG32_ACCUM
	cmpl	$MANT_HI,REG32_ACCUM
	jge	l68
	cmpl	$-MANT_HI,REG32_ACCUM
	jle	l67
	movw	$mval_m_int,mval_w_mvtype(REG64_ARG1)
	movl	REG32_ACCUM,mval_l_m1(REG64_ARG1)
	jmp	l63

l67:	movb	$mval_esign_mask,mval_b_exp(REG64_ARG1)		# set sign bit
	negl	REG32_ACCUM
	jmp	l69

l68:	movb	$0,mval_b_exp(REG64_ARG1)				# clear sign bit
l69:	movw	$mval_m_nm,mval_w_mvtype(REG64_ARG1)
	orb	$69,mval_b_exp(REG64_ARG1)
	movl	REG32_ACCUM,REG32_SCRATCH1
	movl	$0,REG32_ARG2					#set edx to 0 before div
	idivl	ten_dd(REG_IP),REG32_ACCUM
	movl	REG32_ACCUM,mval_l_m1(REG64_ARG1)
	imull	$10,REG32_ACCUM,REG32_ACCUM
	subl	REG32_ACCUM,REG32_SCRATCH1
	imull	$MANT_LO,REG32_SCRATCH1,REG32_SCRATCH1
	movl	REG32_SCRATCH1,mval_l_m0(REG64_ARG1)
	jmp	l63

l66:	movq	REG64_ARG1,REG64_ARG3
	movl	$1,REG32_ARG2
	movq	REG64_ARG0,REG64_ARG1
	movq	REG64_ARG3,REG64_ARG0		#First and fourth args are same
	call	add_mvals
l63:	popq	REG_XFER_TABLE
	leave
	addq	$32,REG_SP			#Pop all incoming arguments
	movq	frame_pointer(REG_IP),REG64_SCRATCH1
	pushq	msf_mpc_off(REG64_SCRATCH1)
	ret

d:	popq	REG_XFER_TABLE
	leave
	addq	$24,REG_SP			#Pop all the arguments except loop address
	ret

# op_forloop ENDP

# END

