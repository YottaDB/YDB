#################################################################
#								#
#	Copyright 2001, 2009 Fidelity Information Services, Inc	#
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
.extern underr

# PUBLIC	op_contain
ENTRY op_contain
	enter	$8, $0
	pushl	%edi
	pushl	%esi
	pushl	%ebx

	movl	%edx, sav_edx(%ebp)
	mv_force_defined %eax, l1
	movl	%eax, sav_eax(%ebp)
	mv_force_str %eax, l2

	movl	sav_edx(%ebp), %edx
	mv_force_defined %edx, l3
	movl	%edx, sav_edx(%ebp)
	mv_force_str %edx, l4

	pushl	$1			# pieces argument but have to pass its addr
	movl	%esp, %edx
	subl	$4, %esp		# returned value
	movl	%esp, %eax
	pushl	%edx			# parm 6
	pushl	%eax			# parm 5
	movl	sav_eax(%ebp), %eax
	movl	sav_edx(%ebp), %edx
	pushl	mval_a_straddr(%eax)	# parm 4
	movl	mval_l_strlen(%eax), %eax
	pushl	%eax			# parm 3
	pushl	mval_a_straddr(%edx)	# parm 2
	movl	mval_l_strlen(%edx), %eax
	pushl	%eax			# parm 1
	call	matchc
	leal	24(%esp), %esp		# remove args
	popl	%eax			# return value
	popl	%edx			# updated pieces value (ignored)
	cmpl	$0, %eax

	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret
# op_contain ENDP

# END
