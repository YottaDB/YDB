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
	.extern	op_unwind

ENTRY	opp_ret
	# Generated code jumps here (it does not call), so the stack is 16 byte aligned and the return stack buffer still
	# has the entry of the call that transferred control to the frame being unwound, if that call left one.
	CHKSTKALIGN				# Verify stack alignment
	movq	frame_pointer(%rip), %rax
	movzbl	msf_flags_off(%rax), %r12d	# Flags of the frame being unwound (%r12 is callee saved; getframe reloads it)
	call	op_unwind
	getframe_ret_or_jmp %r12b
# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
# This marking is not an issue in Linux but is in Windows Subsystem on Linux (WSL) which does not enable executable stack.
.section        .note.GNU-stack,"",@progbits
