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
	.title	opp_tcommit.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_tcommit
#	PAGE	+
	.DATA
.extern	frame_pointer 	# /* :DWORD */

	.text
.extern	op_tcommit

# PUBLIC	opp_tcommit
ENTRY opp_tcommit  #	/* PROC */
	putframe
	addq	$8,REG_SP		#Go past return address
	call	op_tcommit
	getframe
	ret
# opp_tcommit ENDP

# END
