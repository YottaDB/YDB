#################################################################
#								#
#	Copyright 2001, 2007 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	opp_zcont.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_zcont
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	op_zcont

# PUBLIC	opp_zcont
ENTRY opp_zcont
	putframe
	addl	$4,%esp
	call	op_zcont
	getframe
	ret
# opp_zcont ENDP

# END
