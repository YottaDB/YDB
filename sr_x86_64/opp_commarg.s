#################################################################
#								#
# Copyright (c) 2007-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2017-2026 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

	.include "linkage.si"
	.include "g_msf.si"
#	include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	op_commarg

ENTRY	opp_commarg
	putframe
	addq	$8, %rsp		# Burn return PC & 16 byte align stack
	CHKSTKALIGN			# Verify stack alignment
	movq	frame_pointer(%rip), %r12	# Frame before op_commarg (putframe saved %r12, getframe reloads it)
	call	op_commarg
	cmpq	frame_pointer(%rip), %r12
	je	sameframe
	# op_commarg pushed a frame for the indirect code. Our call stays on the return stack buffer for the return from
	# that frame, so jump to its code (see SFF_RSB_ENTRY in stack_frame.h).
	mark_rsb_entry
	getframe_jmp
sameframe:
	# No new frame (for example an indirect GOTO to a label, or XECUTE ""). Use a "ret" so it consumes the entry of
	# our call, which no return from a frame will.
	getframe
	ret				# CONSUMES_OWN_ENTRY (see ci/check_getframe_ret.py)
# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
# This marking is not an issue in Linux but is in Windows Subsystem on Linux (WSL) which does not enable executable stack.
.section        .note.GNU-stack,"",@progbits
