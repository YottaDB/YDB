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
	.title	opp_indfnname.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_indfnname
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	op_indfnname

# PUBLIC	opp_indfnname
ENTRY opp_indfnname
	putframe
	addl	$4,%esp
	call	op_indfnname
	addl	$12,%esp
	getframe
	ret
# opp_indfnname ENDP

# END
