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
	.title	aswp.s
	.sbttl	aswp
.include "g_msf.si"
.include "linkage.si"

	.text
ENTRY aswp
	movl REG32_ARG1, REG32_RET0
	lock
	xchg	(REG64_ARG0),REG32_RET0		# return original value
	ret
