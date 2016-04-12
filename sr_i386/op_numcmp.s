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
	.title	op_numcmp.s
	.sbttl	op_numcmp

#	.386
#	.MODEL	FLAT, C

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
	pushl	%edx
	pushl	%eax
	call	numcmp
	addl	$8,%esp		# restore stack
	cmpl	$0,%eax		# set flags according to result from numcmp
	ret
# op_numcmp ENDP

# END
