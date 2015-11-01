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
	.title	op_follow.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_follow
#	PAGE	+

sav_eax	=	-4
sav_edx	=	-8

	.text
.extern	memvcmp
.extern	n2s

# PUBLIC	op_follow
ENTRY op_follow
	enter	$8, $0
	movl	%eax,sav_eax(%ebp)
	movl	%edx,sav_edx(%ebp)
	mv_force_str %eax, l1
	movl	sav_edx(%ebp),%edx
	mv_force_str %edx, l2
	movl	sav_eax(%ebp),%eax
	movl	sav_edx(%ebp),%edx
	movl	mval_l_strlen(%edx),%ecx
	pushl	%ecx
	pushl	mval_a_straddr(%edx)
	movl	mval_l_strlen(%eax),%ecx
	pushl	%ecx
	pushl	mval_a_straddr(%eax)
	call	memvcmp
	addl	$16,%esp
	cmpl	$0,%eax
	leave
	ret
# op_follow ENDP

# END
