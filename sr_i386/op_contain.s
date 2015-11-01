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
	.title	op_contain.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_contain
#	PAGE	+
	.text
sav_eax	=	-4
sav_edx	=	-8

.extern	matchc
.extern	n2s

# PUBLIC	op_contain
ENTRY op_contain
	enter	$8, $0
	pushl	%edi
	pushl	%esi
	pushl	%ebx

	movl	%eax,sav_eax(%ebp)
	movl	%edx,sav_edx(%ebp)
	mv_force_str %eax, l1
	movl	sav_edx(%ebp),%edx
	mv_force_str %edx, l2
	subl	$4,%esp
	movl	%esp, %eax
	pushl	%eax
	movl	sav_eax(%ebp),%eax
	movl	sav_edx(%ebp),%edx
	pushl	mval_a_straddr(%eax)
	movl	mval_l_strlen(%eax),%eax
	pushl	%eax
	pushl	mval_a_straddr(%edx)
	movl	mval_l_strlen(%edx),%eax
	pushl	%eax
	call	matchc
	leal	20(%esp),%esp
	popl	%eax
	cmpl	$0,%eax

	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret
# op_contain ENDP

# END
