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
	.title	op_zg1.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_zg1
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	golevel

# PUBLIC	op_zg1
ENTRY op_zg1
	putframe
	addq	$8,REG_SP				# burn return pc
	call	golevel
	getframe
	ret
# op_zg1	ENDP

# END
