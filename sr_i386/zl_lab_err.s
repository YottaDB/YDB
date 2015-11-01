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
	.title	zl_lab_err.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	zl_lab_err
#	PAGE	+
	.DATA
.extern	ERR_LABELUNKNOWN
.extern	frame_pointer

	.text
.extern	op_rterror

# PUBLIC	zl_lab_err
ENTRY zl_lab_err
	pushl	$0
	pushl	ERR_LABELUNKNOWN
	call	op_rterror
	addl	$8,%esp
	getframe
	ret
# zl_lab_err ENDP

# END
