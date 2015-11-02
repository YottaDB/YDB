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

JMP_Jb		=	0x0eb
JMP_Jv		=	0x0e9

actual1		=	20
act_cnt		=	16
mask_arg	=	12
ret_val		=	8
rtn_pc		=	4

sav_esi		=	-4
sav_ebx		=	-8
sav_msf		=	-12

# PUBLIC	op_mprofexfun
ENTRY op_mprofexfun
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%esi
	pushl	%ebx
	leal	sav_msf(%ebp),%esp	# establish space for locals
	movl	act_cnt(%ebp),%eax
	addl	$3,%eax
	negl	%eax
	leal	(%esp,%eax,4),%esp	# establish space for temps

	movl	frame_pointer,%edx
	movl	rtn_pc(%ebp),%eax
	cmpb	$JMP_Jv,(%eax)
	je	long
	cmpb	$JMP_Jb,(%eax)
	je	byte_off
error:	pushl	ERR_GTMCHECK
	pushl	$1
	call	rts_error
	addl	$8,%esp
	jmp	retlab

byte_off:
	movl	%eax,msf_mpc_off(%edx)
	addl	$2,msf_mpc_off(%edx)
	jmp	cont

long:	movl	%eax,msf_mpc_off(%edx)
	addl	$5,msf_mpc_off(%edx)
cont:	call	exfun_frame_sp
	leal	ret_val(%ebp),%esi
	movl	%esp,%edi
	movl	act_cnt(%ebp),%eax
	movl	%eax,%ecx
	addl	$3,%ecx
	REP
	movsl
	movl	dollar_truth,%ecx
	andl	$1,%ecx
	pushl	%ecx			# push $T
	addl	$4,%eax
	pushl	%eax			# push total count
	call	push_parm		# push_parm ($T, ret_value, mask, argc [,arg1, arg2, ...]);
done:	movl	frame_pointer,%edx
	movl	msf_temps_ptr_off(%edx),%edi
retlab:	leal	sav_ebx(%ebp),%esp
	movl	rtn_pc(%ebp),%edx
	movl	act_cnt(%ebp),%eax
	addl	$4,%eax
	popl	%ebx
	popl	%esi
	popl	%ebp
	leal	(%esp,%eax,4),%esp
	pushl	%edx
	ret
# op_mprofexfun ENDP

# END
