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
	.title	follow.s
	.sbttl	follow
.include "linkage.si"

#	.386
#	.MODEL	FLAT, C

	.text
.extern	op_follow

# PUBLIC	follow
ENTRY follow
	movl	4(%esp),%eax
	movl	8(%esp),%edx
	call	op_follow
	jle	l1
	movl	$1,%eax
	ret

l1:	movl	$0,%eax
	ret
# follow	ENDP

# END
