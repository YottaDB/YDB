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
	.title	compswap.s
	.sbttl	compswap
.include "linkage.si"

#	.386
#	.MODEL	FLAT, C

	.text
ENTRY compswap
	movl	4(%esp),%edx		# A(latch longword)
	movl	8(%esp),%eax		# comparison value
	movl	12(%esp),%ecx		# replacement value
#	LOCK cmpxchgl %ecx,(%edx)
	lock
	cmpxchgl  %ecx,(%edx)		# compare-n-swap
	jnz	fail
	movl	$1,%eax			# return TRUE
	ret

fail:
	xor	%eax,%eax		# return FALSE
	ret
