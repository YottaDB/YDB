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
	.title	aswp.s
	.sbttl	aswp
.include "linkage.si"

#	.386
#	.MODEL	FLAT, C

	.text
ENTRY aswp
	movl	4(%esp),%edx		# A(latch longword)
	movl	8(%esp),%eax		# replacement value
#	LOCK xchg  (%edx),%eax		# return original value
	lock
	xchgl  (%edx),%eax		# return original value
	ret
