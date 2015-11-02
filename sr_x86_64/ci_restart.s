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
	.title	ci_restart.s
	.sbttl	ci_restart
#	.386
#	.MODEL	FLAT, C
.include "g_msf.si"
.include "linkage.si"

	.DATA
.extern	param_list

	.text
ENTRY ci_restart
	movq	param_list(REG_IP),REG64_ARG2
	movl	8(REG64_ARG2),REG32_ARG3
	cmpl 	$0,REG32_ARG3				# if (argcnt > 0) {
	jle 	L0
	leaq	48(REG64_ARG2),REG64_ARG2
	movq    0(REG64_ARG2),REG64_ARG5
	subl	$1,REG32_ARG3
	leaq	8(REG64_ARG2),REG64_ARG1
	leaq	8(REG_SP),REG64_ARG0
	REP
	movsq						# }
L0:	movq	param_list(REG_IP),REG64_ACCUM
	movl	8(REG64_ACCUM),REG32_ARG4		#argcnt
	movl	40(REG64_ACCUM),REG32_ARG3	#mask
	movq	32(REG64_ACCUM),REG64_ARG2	#retaddr
	movq	24(REG64_ACCUM),REG64_ARG1	#labaddr
	movq	16(REG64_ACCUM),REG64_ARG0	#rtnaddr
	jmp	*(REG64_ACCUM)
	ret

# ci_restart ENDP

# END
