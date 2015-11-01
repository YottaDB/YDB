#################################################################
#								#
#	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_fnlength.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_fnlength
#	PAGE	+
# op_fnlength ( mval * edx, mval * eax) :  eax = length of string of edx
#
#	edx - src - source mval
#	eax - dst - destination mval

	.text
.extern	n2s
.extern i2mval

# PUBLIC	op_fnlength
ENTRY op_fnlength
	mv_if_string %edx, l10
	pushl	%eax
	pushl	%edx
	call	n2s
	popl	%edx
	popl	%eax
l10:	cmpl	$INT_HI,mval_l_strlen(%edx)
	jge	l11			# len too large for mv_i2mval
	pushl	%ebx
	movl	%eax,%ebx
	movl	mval_l_strlen(%edx),%eax
	mv_i2mval %eax, %ebx
	popl	%ebx
	ret
l11:	pushl	mval_l_strlen(%edx)
	pushl	%eax
	call	i2mval
	addl	$8,%esp
	ret
# op_fnlength ENDP

# END
