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
	.title	op_restartpc.s
	.sbttl	op_restartpc

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"

	.DATA
.extern	restart_pc

	.text
# PUBLIC	op_restartpc
ENTRY op_restartpc
	movl	(%esp),%eax
	subl	$6,%eax
	movl	%eax,restart_pc
	ret
# op_restartpc ENDP

# END
