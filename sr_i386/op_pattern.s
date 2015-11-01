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
	.title	op_pattern.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_pattern
#	PAGE	+
	.text
.extern	do_patfixed
.extern	do_pattern

# PUBLIC	op_pattern
ENTRY op_pattern
	pushl	%edx
	pushl	%eax
	movl	mval_a_straddr(%edx),%eax
	cmpb	$0,(%eax)
	je	l1
	call	do_patfixed
	jmp	l2

l1:	call	do_pattern
l2:	addl	$8,%esp
	cmpl	$0,%eax
	ret
# op_pattern ENDP

# END
