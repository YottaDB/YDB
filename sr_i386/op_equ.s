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
	.title	op_equ.s
	.sbttl	op_equ
.include "linkage.si"

#	.386
#	.MODEL	FLAT, C

	.text
.extern	is_equ

# PUBLIC	op_equ
ENTRY op_equ
	pushl	%edx
	pushl	%eax
	call	is_equ
	addl	$8,%esp
	cmpl	$0,%eax
	ret
# op_equ	ENDP

# END
