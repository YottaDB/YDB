#################################################################
#								#
# Copyright (c) 2010-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
#
#      Args:
#	 mval * (routine name)
#	 mval * (label name)
#	 int (offset from label)
#	 int (stack frame nesting level to which to transfer control)
#
	.include "linkage.si"
	.include "g_msf.si"
	.include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	op_zgoto

ENTRY	opp_zgoto
	putframe
	addq	$8, REG_SP		# Burn return address & 16 byte align stack
	CHKSTKALIGN			# Verify stack alignment
	call	op_zgoto		# All 4 arg regs passed to opp_zgoto
	getframe
	ret
