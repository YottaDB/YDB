#################################################################
#								#
#	Copyright 2001, 2007 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	opp_trestart.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_trestart
#	PAGE	+
	.DATA
.extern	frame_pointer 	# /*:DWORD */

	.text
.extern	op_trestart

# PUBLIC	opp_trestart
ENTRY opp_trestart  #	/* PROC */
	putframe
	addl	$4,%esp
	call	op_trestart
	addl	$4,%esp
	getframe
	ret
# opp_trestart ENDP

# END
