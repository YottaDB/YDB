#################################################################
#								#
# Copyright (c) 2001-2020 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	caller_id.s
	.sbttl	caller_id

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"

	.text

# PUBLIC	caller_id
ENTRY caller_id
	movl 4(%esp),%ecx	# get extra frames count
	movl %ebp,%eax		# copy frame pointer
	cmpl $0,%ecx		# check for extra frames
	jle L2
L1:	decl %ecx		# reduce count of extra frames
	movl 0(%eax),%eax	# get previous stack frame
	cmpl $0,%ecx		# check for additional extra frames
	jg L1
L2:	movl 4(%eax),%eax	# address of caller's return address
	ret
# caller_id ENDP

# END
