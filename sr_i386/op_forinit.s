#################################################################
#								#
#	Copyright 2001, 2008 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_forinit.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.INCLUDE	"mval_def.si"

	.sbttl	op_forinit
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	numcmp
.extern	s2n
.extern underr

# PUBLIC	op_forinit
ENTRY op_forinit
	movl	frame_pointer,%edx
	popl	msf_mpc_off(%edx)
	movl	4(%esp),%eax
	mv_force_defined %eax, l0
	movl	%eax, 4(%esp)
	mv_force_num %eax, t2
	movl	4(%esp),%eax
	cmpl	$0,mval_l_m1(%eax)
	js	l2
	mv_if_int %eax, l1
	testb	$mval_esign_mask,mval_b_exp(%eax)
	jne	l2
l1:	movl	8(%esp),%eax
	movl	%eax,4(%esp)
	call	numcmp
	addl	$12,%esp
	movl	frame_pointer,%edx
	pushl	msf_mpc_off(%edx)
	cmpl	$0,%eax
	ret

l2:	movl	8(%esp),%eax
	pushl	%eax
	call	numcmp
	addl	$16,%esp
	movl	frame_pointer,%edx
	pushl	msf_mpc_off(%edx)
	cmpl	$0,%eax
	ret
# op_forinit ENDP

# END
