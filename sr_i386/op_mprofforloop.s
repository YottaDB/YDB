#################################################################
#								#
#	Copyright 2001 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_mprofforloop.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_mprofforloop
#	PAGE	+
#	Called with the stack contents:
#		call return
#		ptr to index mval
#		ptr to step mval
#		ptr to terminator mval
#		loop address
	.DATA
.extern	frame_pointer

# ten_dd	DD	10
ten_dd:
.long	10

	.text
.extern	add_mvals
.extern	numcmp
.extern	s2n
.extern pcurrpos

term	=	12
step	=	8
indx	=	4
MPR_INTOFOR	=	0x2
MPR_OUTOFFOR	=	0x1

# PUBLIC	op_mprofforloop
ENTRY op_mprofforloop
	movl	frame_pointer,%edx
	popl	msf_mpc_off(%edx)
	enter	$0, $0
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	movl	indx(%ebp),%esi
	mv_force_num %esi, l1
	movl	indx(%ebp),%esi
	movl	step(%ebp),%edi
	movb	mval_b_mvtype(%esi),%al
	movb	mval_b_mvtype(%edi),%dl
	andb	%dl,%al
	testb	$mval_m_int_without_nm,%al
	je	L66
	movl	mval_l_m1(%esi),%eax
	addl	mval_l_m1(%edi),%eax
	cmpl	$MANT_HI,%eax
	jge	L68
	cmpl	$-MANT_HI,%eax
	jle	L67
	movb	$mval_m_int,mval_b_mvtype(%esi)
	movl	%eax,mval_l_m1(%esi)
	jmp	L63

L67:	movb	$mval_esign_mask,mval_b_exp(%esi)	# set sign bit
	negl	%eax
	jmp	L69

L68:	movb	$0,mval_b_exp(%esi)			# clear sign bit
L69:	movb	$mval_m_nm,mval_b_mvtype(%esi)
	orb	$69,mval_b_exp(%esi)			# set exponent field
	movl	%eax,%ebx
	movl	$0,%edx
	idivl	ten_dd,%eax
	movl	%eax,mval_l_m1(%esi)
	imull	$10,%eax,%eax
	subl	%eax,%ebx
	imull	$MANT_LO,%ebx,%ebx
	movl	%ebx,mval_l_m0(%esi)
	jmp	L63

L66:	pushl	%esi
	pushl	$0
	pushl	%edi
	pushl	%esi
	call	add_mvals
	addl	$16,%esp
	movl	indx(%ebp),%esi
L63:	movl	step(%ebp),%edi
	testb	$mval_m_int_without_nm,mval_b_mvtype(%edi)
	jne	a
	cmpb	$0,mval_b_exp(%edi)
	jl	b
	jmp	a2

a:	cmpl	$0,mval_l_m1(%edi)
	jl	b
a2:	movl	term(%ebp),%edi
	jmp	e

b:	movl	%esi,%edi		# if step is negative, reverse compare
	movl	term(%ebp),%esi
e:	# compare indx and term
	movb	mval_b_mvtype(%esi),%al
	movb	mval_b_mvtype(%edi),%dl
	andb	%dl,%al
	testb	$2,%al
	je	ccmp
	movl	mval_l_m1(%esi),%eax
	subl	mval_l_m1(%edi),%eax
	jmp	tcmp

ccmp:	pushl	%edi
	pushl	%esi
	call	numcmp
	addl	$8,%esp
	cmpl	$0,%eax
tcmp:	jle	d
	movl	indx(%ebp),%esi
	movl	step(%ebp),%edi
	movb	mval_b_mvtype(%esi),%al
	movb	mval_b_mvtype(%edi),%dl
	andb	%dl,%al
	testb	$mval_m_int_without_nm,%al
	je	l66
	movl	mval_l_m1(%esi),%eax
	subl	mval_l_m1(%edi),%eax
	cmpl	$MANT_HI,%eax
	jge	l68
	cmpl	$-MANT_HI,%eax
	jle	l67
	movb	$mval_m_int,mval_b_mvtype(%esi)
	movl	%eax,mval_l_m1(%esi)
	jmp	l63

l67:	movb	$mval_esign_mask,mval_b_exp(%esi)	# set sign bit
	negl	%eax
	jmp	l69

l68:	movb	$0,mval_b_exp(%esi)			# clear sign bit
l69:	movb	$mval_m_nm,mval_b_mvtype(%esi)
	orb	$69,mval_b_exp(%esi)			# set exponent field
	movl	%eax,%ebx
	movl	$0,%edx
	idivl	ten_dd,%eax
	movl	%eax,mval_l_m1(%esi)
	imull	$10,%eax,%eax
	subl	%eax,%ebx
	imull	$MANT_LO,%ebx,%ebx
	movl	%ebx,mval_l_m0(%esi)
	jmp	l63

l66:	pushl	%esi
	pushl	$1
	pushl	%edi
	pushl	%esi
	call	add_mvals
	addl	$16,%esp
l63:	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	addl	$16,%esp			# remove op_mprofforloop arguments
	movl	frame_pointer,%edx
	pushl	msf_mpc_off(%edx)
	pushl	$MPR_OUTOFFOR
	call	pcurrpos			# to count fors properly
	addl 	$4,%esp
	ret

d:	popl	%ebx
	popl	%esi
	popl	%edi
	pushl	$MPR_INTOFOR
	call	pcurrpos
	addl 	$4,%esp
	leave
	addl	$12,%esp			# remove term, step, indx; leave loop addr
	ret

# op_mprofforloop ENDP

# END
