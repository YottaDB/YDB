#################################################################
#								#
#	Copyright 2007, 2010 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_retarg.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_retarg
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	unw_retarg

# PUBLIC	op_retarg
ENTRY op_retarg
	movq	REG64_RET0,REG64_ARG0
	movq	REG64_RET1,REG64_ARG1
	call	unw_retarg
	addq    $8, REG_SP
	getframe
	ret
# op_retarg ENDP

# END
