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
	.title	opp_fngvget1.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_fngvget1
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	op_fngvget1

# PUBLIC	opp_fngvget1
ENTRY opp_fngvget1
	putframe		# really just to save return address
	addl	$4,%esp		# remove return address
	call	op_fngvget1
	addl	$4,%esp		# remove argument
	getframe		# restore some stuff and push return address
	ret
# opp_fngvget1 ENDP

# END
