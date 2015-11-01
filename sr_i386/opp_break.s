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
	.title	opp_break.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_break
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	op_break

# PUBLIC	opp_break
ENTRY opp_break
	putframe
	addl	$4,%esp
	call	op_break
	getframe
	ret
# opp_break ENDP

# END
