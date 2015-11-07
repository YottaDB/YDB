#################################################################
#								#
#	Copyright 2007, 2013 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_mprofexfun.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_mprofexfun
#	PAGE	+
#	call op_mprofexfun with the following stack:
#
#		return PC
#		ret_value address
# 		mask
#		actualcnt
#		actual1 address
#		actual2 address
#		...

	.DATA
.extern	ERR_GTMCHECK
.extern	dollar_truth
.extern	frame_pointer

	.text
.extern	exfun_frame_sp
.extern	push_parm
.extern	rts_error

arg2_off	=	-72
arg1_off	=	-64
arg0_off	=	-56
act_cnt		=	-48
mask_arg	=	-44
ret_val		=	-40
rtn_pc		=	-32

sav_esi		=	-8
sav_ebx		=	-16
sav_msf		=	-24

# PUBLIC	op_mprofexfun
ENTRY op_mprofexfun
	pushq	REG_XFER_TABLE
	enter   $80,$0
        movq    16(REG_FRAME_POINTER), REG64_SCRATCH1
	movq	REG64_SCRATCH1,rtn_pc(REG_FRAME_POINTER)		# Save return address
        movq    REG64_ARG0,ret_val(REG_FRAME_POINTER)			# Save incoming arguments
        movl    REG32_ARG2,mask_arg(REG_FRAME_POINTER)
        movl    REG32_ARG3,act_cnt(REG_FRAME_POINTER)
	movq    REG64_ARG4,arg0_off(REG_FRAME_POINTER)
        movq    REG64_ARG5,arg1_off(REG_FRAME_POINTER)
	movq    frame_pointer(REG_IP),REG64_ARG2
        movq    rtn_pc(REG_FRAME_POINTER),REG64_ACCUM			#Verify the immediate instruction after this
	cmpb	$JMP_Jv,(REG64_ACCUM)					# function call
	je	long

error:	movl	ERR_GTMCHECK(REG_IP),REG32_ARG1
        movl	$1,REG32_ARG0
	movb	$0,REG8_ACCUM             # variable length argument
	call	rts_error
	jmp	retlab

long:	movq    REG64_ACCUM,msf_mpc_off(REG64_ARG2)
	addq	REG64_ARG1,msf_mpc_off(REG64_ARG2)
cont:	call	exfun_frame_sp
	movq    frame_pointer(REG_IP),REG64_SCRATCH1

        movl    act_cnt(REG_FRAME_POINTER),REG32_ACCUM
        cmpl    $0,REG32_ACCUM                          #arg0, arg1, arg2 are stored in rbp
        je      no_arg
        cmpl    $1,REG32_ACCUM                          #We have only one register free for push_param args
        je      arg0
        cmpl	$2,REG32_ACCUM
        je	arg1
	cltq
	leaq	(REG_FRAME_POINTER,REG64_ACCUM,8),REG64_SCRATCH1
again:	pushq	(REG64_SCRATCH1)
        subq    $8,REG64_SCRATCH1
        subl    $1,REG32_ACCUM
        cmpl    $2,REG32_ACCUM
        jg	again
arg1:   pushq   arg1_off(REG_FRAME_POINTER)
arg0:   movq    arg0_off(REG_FRAME_POINTER),REG64_ARG5  #Only one argument which can be fitted into REG5
no_arg: movl    act_cnt(REG_FRAME_POINTER), REG32_ARG4  #Actual Arg cnt
	movl    mask_arg(REG_FRAME_POINTER),REG32_ARG3	#Mask
        movq    ret_val(REG_FRAME_POINTER), REG64_ARG2	#ret_value
	movl	dollar_truth(REG_IP),REG32_ACCUM
	andl	$1,REG32_ACCUM
	movl    REG32_ACCUM,REG32_ARG1                  #$T
	movl	act_cnt(REG_FRAME_POINTER),REG32_ACCUM
	addl	$4,REG32_ACCUM
	movl    REG32_ACCUM, REG32_ARG0                 #Totalcount = Act count +4
	movb    $0,REG8_ACCUM             		# variable length argument
	call	push_parm				# push_parm (total, $T, ret_value, mask, argc [,arg1, arg2, ...]);
done:	movq    frame_pointer(REG_IP),REG64_SCRATCH1
	movq	msf_temps_ptr_off(REG64_SCRATCH1),REG_FRAME_TMP_PTR
retlab:	leave
	movq    REG64_SCRATCH1,REG_FRAME_POINTER
	popq	REG_XFER_TABLE
	ret
# op_mprofexfun ENDP

# END
