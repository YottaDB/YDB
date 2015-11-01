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
	.title	caller_id.s
	.sbttl	caller_id

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"

	.text

# PUBLIC	caller_id
ENTRY caller_id
	movl 4(%ebp),%eax	# address of caller's return address
	ret
# caller_id ENDP

# END
