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
	.title	opp_newintrinsic.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_newintrinsic
#	PAGE	+
	.DATA
.extern	frame_pointer #	/* :DWORD */

	.text
.extern	op_newintrinsic

# PUBLIC	opp_newintrinsic
ENTRY opp_newintrinsic 	# /* PROC */
	putframe
	addl	$4,%esp
	call	op_newintrinsic
	addl	$4,%esp
	getframe
	ret
# opp_newintrinsic ENDP

# END
