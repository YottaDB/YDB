#################################################################
#								#
#	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	opp_rterror.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_rterror
#	PAGE	+
	.DATA
.extern	frame_pointer #	/* :DWORD */

	.text
.extern	op_rterror

# PUBLIC	opp_rterror
ENTRY opp_rterror  #	/* PROC */
	putframe
	addl	$4,%esp		# burn return PC
	call	op_rterror
	addl	$8,%esp		# remove args from stack
	getframe
	ret
# opp_rterror ENDP

# END
