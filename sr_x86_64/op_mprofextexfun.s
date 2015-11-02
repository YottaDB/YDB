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
	.title	op_mprofextexfun.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"

	.sbttl	op_mprofextexfun
#	PAGE	+

#	call op_mprofextexfun with the following stack:
#
#		return PC
#		routine
#		label
#		ret_value address
#		mask
#		actualcnt
#		actual1 address
#		actual2 address
#		...

	.DATA
.extern	ERR_FMLLSTMISSING
.extern	ERR_GTMCHECK
.extern	ERR_LABELUNKNOWN
.extern	dollar_truth
.extern	frame_pointer

	.text
.extern	auto_zlink
.extern	new_stack_frame_sp
.extern	push_parm
.extern	rts_error

arg0_off        =       -56
act_cnt         =       -48
mask_arg        =       -44
ret_val         =       -40
label_arg       =       -32
routine         =       -24
sav_ebx		=	-8
sav_msf		=	-16

# PUBLIC	op_mprofextexfun
ENTRY op_mprofextexfun
	putframe
	addq	$8,REG_SP		# burn return PC
        pushq  	REG_XFER_TABLE
	enter   $64,$0
        movq    REG64_ARG0,routine(REG_FRAME_POINTER)
        movq    REG64_ARG1,label_arg(REG_FRAME_POINTER)
        movq    REG64_ARG2,ret_val(REG_FRAME_POINTER)
        movl    REG32_ARG3,mask_arg(REG_FRAME_POINTER)
        movl    REG32_ARG4,act_cnt(REG_FRAME_POINTER)
	movq	REG64_ARG5,arg0_off(REG_FRAME_POINTER)
	cmpq	$0,REG64_ARG0
	je	l3
	cmpq    $0,REG64_ARG1
        je      l5

l1:	movq    0(REG64_ARG1),REG64_ACCUM
        cmpq    $0,REG64_ACCUM
        je      l5
        movq    mrt_ptext_adr(REG64_ARG0),REG_XFER_TABLE
	movslq  0(REG64_ACCUM),REG64_ARG2
        addq    REG_XFER_TABLE,REG64_ARG2

	addq	$8,REG64_ARG1			# labaddr += 8, to point to has_parms
	cmpl	$0,0(REG64_ARG1)		# if has_parms == 0, then issue an error
	je	l6

	movq    mrt_lnk_ptr(REG64_ARG0),REG64_ARG1
	call	new_stack_frame_sp

        movl    act_cnt(REG_FRAME_POINTER),REG32_ACCUM
       	cmpl    $0,REG32_ACCUM
        je	no_arg
	cmpl    $1,REG32_ACCUM
        je	arg_1
	cltq
	leaq    (REG_FRAME_POINTER,REG64_ACCUM,8),REG64_SCRATCH1
again:  pushq	(REG64_SCRATCH1)
        subq    $8,REG64_SCRATCH1
        subl    $1,REG32_ACCUM
        cmpl    $1,REG32_ACCUM
        jg	again
arg_1:  movq    arg0_off(REG_FRAME_POINTER),REG64_ARG5
no_arg: movl    act_cnt(REG_FRAME_POINTER),REG32_ARG4
	movl    mask_arg(REG_FRAME_POINTER),REG32_ARG3
        movq    ret_val(REG_FRAME_POINTER),REG64_ARG2
	movl	dollar_truth(REG_IP),REG32_ARG1
	andl	$1,REG32_ARG1
	movl	act_cnt(REG_FRAME_POINTER),REG32_ARG0
	addl	$4,REG32_ARG0			# include: $T(just pushed) plus other 3
	movb    $0,REG8_ACCUM			# variable length argument
	call	push_parm			# push_parm (total_cnt,$T, ret_value, mask, argc [,arg1, arg2, ...]);
retlab:	leave
	popq	REG_XFER_TABLE
	getframe
	ret

l3:	cmpq	$0,REG64_ARG1
	jne	l5
	subq	$8,REG_SP
	movq	REG_SP,REG64_ACCUM
	movq	REG64_ACCUM,REG64_ARG1
        movq    frame_pointer(REG_IP),REG64_ARG2
        movq	msf_mpc_off(REG64_ARG2),REG64_ARG0
	call	auto_zlink
	cmpq	$0,REG64_RET0
	je	l4
	movq    REG64_RET0,REG64_ARG0
	popq	REG64_ARG1
	cmpq	$0,REG64_ARG1
	jne	l1
l4:	movl	ERR_GTMCHECK(REG_IP),REG32_ARG1
        movl	$1,REG32_ARG0
	movb    $0,     REG8_ACCUM             # variable length argument
	call	rts_error
	jmp	retlab

l5:	movl	ERR_LABELUNKNOWN(REG_IP),REG32_ARG1
        movl	$1,REG32_ARG0
	movb    $0,	REG8_ACCUM             # variable length argument
	call	rts_error
	jmp	retlab

l6:	movl	ERR_FMLLSTMISSING(REG_IP),REG32_ARG1
        movl	$1,REG32_ARG0
	movb	$0,	REG8_ACCUM             # variable length argument
	call	rts_error
	jmp	retlab

# op_mprofextexfun ENDP

# END
