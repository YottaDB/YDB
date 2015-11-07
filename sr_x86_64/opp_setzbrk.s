#################################################################
#								#
#	Copyright 2013, 2014 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	opp_setzbrk.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_setzbrk
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	op_setzbrk

# PUBLIC	opp_setzbrk
ENTRY opp_setzbrk
	putframe
	addq	$8,REG_SP   # burn return PC
	call	op_setzbrk
	getframe
	ret
# opp_setzbrk ENDP

# END
