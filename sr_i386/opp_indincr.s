#################################################################
#								#
#	Copyright 2004 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	opp_indincr.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_indincr
#	PAGE	+
	.DATA
.extern	frame_pointer 	# /* :DWORD */

	.text
.extern	op_indincr

# PUBLIC	opp_indincr
ENTRY opp_indincr  	# /* PROC */
	putframe
	addl	$4,%esp
	call	op_indincr
	addl	$12,%esp
	getframe
	ret
# opp_indincr ENDP

# END
