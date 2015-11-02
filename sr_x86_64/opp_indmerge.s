#################################################################
#								#
#	Copyright 2007 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	opp_indmerge.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_indmerge
#	PAGE	+
	.DATA
.extern	frame_pointer 	# /* :DWORD */

	.text
.extern	op_indmerge

# PUBLIC	opp_indmerge
ENTRY opp_indmerge  	# /* PROC */
	putframe
	addq	$8,REG_SP   # burn return PC
	call	op_indmerge
	getframe
	ret
# opp_indmerge ENDP

# END
