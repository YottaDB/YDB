#################################################################
#								#
#	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	#
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

.include "linkage.si"
	.INCLUDE	"g_msf.si"

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

Grp5_Prefix	=	0x0ff		# escape for CALL_Ev opcode
CALL_Ev		=	0x010		# CALL_Ev part of ModR/M byte
reg_opcode_mask	=	0x038
ISFORMAL	=	0x0244		# xf_isformal*4

actual1		=	24
act_cnt		=	20
mask_arg	=	16
ret_val		=	12
label_arg	=	8
routine		=	4

sav_ebx		=	-4
sav_msf		=	-8

# PUBLIC	op_mprofextexfun
ENTRY op_mprofextexfun
	putframe
	addl	$4,%esp			# burn return PC
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	leal	sav_msf(%ebp),%esp	# establish space for locals
	movl	act_cnt(%ebp),%eax
	addl	$3,%eax
	negl	%eax
	leal	(%esp,%eax,4),%esp	# add space for temps

	movl	routine(%ebp),%edx
	movl	label_arg(%ebp),%eax
	cmpl	$0,%eax
	je	l3
l1:	movl	(%eax),%ebx
	addl	mrt_curr_ptr(%edx),%ebx
	addl	%edx,%ebx
	cmpb	$Grp5_Prefix,0(%ebx)
	jne	l6
	movb	1(%ebx),%cl
	andb	$reg_opcode_mask,%cl
	cmpb	$CALL_Ev,%cl
	jne	l6
	cmpl	$ISFORMAL,2(%ebx)
	jne	l6
	pushl	%ebx
	pushl	$0
	pushl	%edx
	call	new_stack_frame_sp
	addl	$12,%esp
	movl	frame_pointer,%edx
	movl	msf_old_frame_off(%edx),%eax
	movl	%eax,frame_pointer
	movl	%edx,sav_msf(%ebp)
	leal	ret_val(%ebp),%esi
	movl	%esp,%edi
	movl	act_cnt(%ebp),%eax
	movl	%eax,%ecx
	addl	$3,%ecx			# include: A(ret_value), mask, argc
	REP
	movsl
	movl	dollar_truth,%ecx
	andl	$1,%ecx
	pushl	%ecx
	addl	$4,%eax			# include: $T(just pushed) plus other 3
	pushl	%eax			# push total count
	call	push_parm		# push_parm ($T, ret_value, mask, argc [,arg1, arg2, ...]);
	movl	sav_msf(%ebp),%eax
	movl	%eax,frame_pointer
	orw	$SFT_EXTFUN,msf_typ_off(%eax)
retlab:	leal	sav_ebx(%ebp),%esp
	movl	act_cnt(%ebp),%eax
	addl	$5,%eax
	popl	%ebx
	popl	%ebp
	leal	(%esp,%eax,4),%esp
	getframe
	ret

l3:	cmpl	$0,%edx
	jne	l5
	subl	$4,%esp
	movl	%esp,%eax
	pushl	%eax
	movl	frame_pointer,%edx
	pushl	msf_mpc_off(%edx)
	call	auto_zlink
	addl	$8,%esp
	cmpl	$0,%eax
	je	l4
	movl	%eax,%edx
	popl	%eax
	cmpl	$0,%eax
	jne	l1
l4:	pushl	ERR_GTMCHECK
	pushl	$1
	call	rts_error
	jmp	retlab

l5:	pushl	ERR_LABELUNKNOWN
	pushl	$1
	call	rts_error
	jmp	retlab

l6:	pushl	ERR_FMLLSTMISSING
	pushl	$1
	call	rts_error
	jmp	retlab

# op_mprofextexfun ENDP

# END
