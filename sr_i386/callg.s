#################################################################
#								#
#	Copyright 2001, 2006 Fidelity Information Services, Inc	#
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
# and that it follows our version of the VMS calling conventions - i.e. that
# a count of the arguments following is actually the first thing on the stack.

routarg =	8
argsarg =	12
cntsav  =	-4
	.text
ENTRY callg
	enter	$4,$0
	pushl	%edi
	pushl	%esi
	pushl	%ebx

	movl	routarg(%ebp),%edx	# routine to call
	movl	argsarg(%ebp),%esi	# argument list
	movl	(%esi),%ecx
	movl	%ecx,cntsav(%ebp)	# save following argument count
	addl	$4,%esi			# skip argument count
	movl	%ecx, %eax
	negl	%eax
	leal	(%esp,%eax,4),%esp
	movl	%esp,%edi
	pushl	%ecx
	rep
	movsl
	call	*%edx
	popl	%edx			# discard possibly modified count
	movl	cntsav(%ebp),%edx	# edx to preserve return value in eax
	leal	(%esp,%edx,4),%esp

	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret
