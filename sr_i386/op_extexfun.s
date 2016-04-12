#################################################################
#								#
#	Copyright 2001, 2012 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_extexfun.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_extexfun
#	PAGE	+

#	call op_extexfun with the following stack:
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
.extern	new_stack_frame
.extern	push_parm
.extern	rts_error

actual1		=	24
act_cnt		=	20
mask_arg	=	16
ret_val		=	12
label_arg	=	8
routine		=	4

sav_ebx		=	-4
sav_msf		=	-8

# PUBLIC	op_extexfun
ENTRY op_extexfun
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

l1:	pushl	%eax			# save labaddr
	movl	(%eax),%eax		# get offset to line number entry
	cmpl	$0,%eax
	je	l5
	addl	mrt_curr_ptr(%edx),%eax
	addl	%edx,%eax		# get the pointer to line number entry
	movl	(%eax),%ebx		# get line number
	addl	mrt_curr_ptr(%edx),%ebx
	addl	%edx,%ebx
	popl	%eax			# restore labaddr

	cmpl	$0,4(%eax)		# labaddr += 4, to point to has_parms; then *has_parms
	je	l6			# if has_parms == 0, then issue an error

	pushl	%ebx
	pushl	$0
	pushl	%edx
	call	new_stack_frame
	addl	$12,%esp

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

# op_extexfun ENDP

# END
