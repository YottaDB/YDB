#################################################################
#								#
#	Copyright 2010, 2011 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	opp_zgoto.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_zgoto
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	op_zgoto

# PUBLIC	opp_zgoto
ENTRY	opp_zgoto
	putframe
	addl	$4,%esp			# burn return pc
	call	op_zgoto
	addl	$16,%esp		# burn passed in 4 args
	getframe
	ret
# opp_zgoto	ENDP

# END
