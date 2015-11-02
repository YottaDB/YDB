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

#      Args:
#        level   - stack frame nesting level to which to transfer control
#        rhdaddr - address of routine header of routine containing the label
#        labaddr - address of address of offset into routine to which to transfer cont
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
	addq	$8,REG_SP	# Burn return address
	call	op_zgoto	# All three arg regs passed to zgoto_level
	getframe
	ret
# opp_zgoto ENDP

# END
