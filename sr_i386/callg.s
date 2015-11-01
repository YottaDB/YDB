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
	.title	callg.s
	.sbttl	callg

.include "linkage.si"

#	.386
#	.MODEL	FLAT, C

# Assembler routine to immitate the VAX CALLG instruction, which can redirect
# a routine calls argument list to a prepared list in memory.
# This implementation has the caveat that the argument list must be integers,
# and that it follows our VARARGS C conventions - i.e. that a count of the
# arguments is actually the first thing on the stack.

	.text
# PUBLIC	callg
# callg	PROC
ENTRY callg
	enter	$0,$0
	pushl	%edi
	pushl	%esi
	pushl	%ebx

	movl	8(%ebp),%edx		# routine to call
	movl	12(%ebp),%esi		# argument list
	movl	(%esi),%ecx
	addl	$4,%esi
	movl	%ecx, %eax
	negl	%eax
#	lea	esp, [esp][4*eax]
	leal	(%esp,%eax,4),%esp
	movl	%esp,%edi
	pushl	%ecx
#	REP movsd
	rep
	movsl
	call	*%edx
	popl	%edx			# preserve return value in eax
#	lea	esp, [esp][4*edx]
	leal	(%esp,%edx,4),%esp

	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret
# callg	ENDP

# END
