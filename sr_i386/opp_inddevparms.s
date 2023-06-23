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
	.title	opp_inddevparms.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_inddevparms
#	PAGE	+
	.DATA
.extern	frame_pointer 	# /* :DWORD */

	.text
.extern	op_inddevparms

# PUBLIC	opp_inddevparms
ENTRY opp_inddevparms  	# /* PROC */
	putframe
	addl	$4,%esp
	call	op_inddevparms
	addl	$12,%esp
	getframe
	ret
# opp_inddevparms ENDP

# END
