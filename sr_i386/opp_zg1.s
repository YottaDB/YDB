#################################################################
#								#
#	Copyright 2010 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	opp_zg1.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_zg1
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	op_zg1

# PUBLIC	opp_zg1
ENTRY	opp_zg1
	putframe
	addl	$4,%esp			# burn return pc
	call	op_zg1
	addl	$4,%esp			# burn passed-in arg
	getframe
	ret
# opp_zg1	ENDP

# END
