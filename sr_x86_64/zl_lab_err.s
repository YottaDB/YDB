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
	addq    $8,%rsp  # burn the return pc
	movl   $0,REG32_ARG1
        movl   ERR_LABELUNKNOWN(REG_IP),REG32_ARG0
	call	op_rterror
	getframe
	ret
# zl_lab_err ENDP

# END
