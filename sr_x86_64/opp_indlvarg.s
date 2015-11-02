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
	.title	opp_indlvarg.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_indlvarg
#	PAGE	+
	.DATA
.extern	frame_pointer 	# /* :DWORD */

	.text
.extern	op_indlvarg

# PUBLIC	opp_indlvarg
ENTRY opp_indlvarg  	# /* PROC */
	putframe
	addq	$8,REG_SP   # burn return PC
	call	op_indlvarg
	getframe
	ret
# opp_indlvarg ENDP

# END
