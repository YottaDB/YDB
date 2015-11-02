#################################################################
#								#
#	Copyright 2007 Fidelity Information Services, Inc	#
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

.include "g_msf.si"
.include "linkage.si"

#	.386
#	.MODEL	FLAT, C

	.text
.extern	op_follow

# PUBLIC	follow
ENTRY follow
	movq	REG64_ARG0,REG64_RET0
	movq	REG64_ARG1,REG64_RET1
	call	op_follow
	jle	l1
	movq	$1,REG64_RET0
	ret

l1:	movq	$0,REG64_RET0
	ret
# follow	ENDP

# END
