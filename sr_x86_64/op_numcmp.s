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
	.title	op_numcmp.s
	.sbttl	op_numcmp

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"

#	op_numcmp calls numcmp to compare two mvals
#
#	entry:
#		eax	mval *u
#		edx	mval *v
#
#	exit:
#		condition codes set according to value of
#			numcmp (u, v)

	.text
.extern	numcmp

# PUBLIC	op_numcmp
ENTRY op_numcmp
	movq	REG64_RET1,REG64_ARG1
	movq	REG64_RET0,REG64_ARG0
	call	numcmp
	cmpl	$0,REG32_ACCUM		# set flags according to result from numcmp
	ret
# op_numcmp ENDP

# END
