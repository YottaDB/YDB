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
	.title	opp_indtext.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_indtext
#	PAGE	+
	.DATA
.extern	frame_pointer 	# /* :DWORD */

	.text
.extern	op_indtext

# PUBLIC	opp_indtext
ENTRY opp_indtext  	# /* PROC */
	putframe
	addl	$4,%esp		# burn return PC
	call	op_indtext
	addl	$16,%esp	# remove args from stack
	getframe
	ret
# opp_indtext ENDP

# END
