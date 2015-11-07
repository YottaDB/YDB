#################################################################
#								#
#	Copyright 2007, 2013 Fidelity Information Services, Inc	#
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
.include "g_msf.si"
.include "linkage.si"

#	.386
#	.MODEL	FLAT, C

	.text
ENTRY compswap
	# A(latch longword)	Arg0
	# comparison value	Arg1
	# replacement value	Arg2
	# cmpxchg will compare REG32_RET0 i.e EAX with 1st arg so copy
	# comparison value to EAX
	movl	REG32_ARG1,REG32_RET0
	lock
	cmpxchgl  REG32_ARG2,(REG64_ARG0)		# compare-n-swap
	jnz	fail
	movl	$1,REG32_RET0			# return TRUE
	ret

fail:
	xor	REG32_RET0,REG32_RET0		# return FALSE
	pause
	ret
