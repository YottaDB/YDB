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
	.title	op_namechk.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_namechk
#	PAGE	+
	.text
.extern	underr

# PUBLIC	op_namechk
ENTRY op_namechk
	mv_if_defined %eax, clab
	pushl	%eax
	call	underr
	addl	$4,%esp
clab:	ret
# op_namechk ENDP

# END
