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
	.title	opp_xnew.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_xnew
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	op_xnew

# PUBLIC	opp_xnew
ENTRY opp_xnew
	putframe
	addq	$8,REG_SP                 # burn return PC
	movb    $0,REG8_ACCUM             # variable length argument
	call	op_xnew
	getframe
	ret
# opp_xnew ENDP

# END
