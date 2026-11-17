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

	.include "g_msf.si"
	.include "linkage.si"
#	include "debug.si"

	.text
	.extern	opp_ret
#
# This routine is only ever "returned to" since its address stored into the mpc field of stack frames
# during certain types of error recovery. Because of that, there is no "return address" on the stack.
# Like generated code, this routine jumps to opp_ret, which expects no return address on the stack.
# So this routine has no caller and no return. Nevertheless, we check the stack alignment to
# verify it before passing control to opp_ret for the unwind.
#
ENTRY	pseudo_ret
	CHKSTKALIGN			# Verify stack alignment
	jmp	opp_ret				# opp_ret is entered with a jump, like from generated code
# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
# This marking is not an issue in Linux but is in Windows Subsystem on Linux (WSL) which does not enable executable stack.
.section        .note.GNU-stack,"",@progbits
