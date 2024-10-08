#################################################################
#								#
# Copyright (c) 2007-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries.	#
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

# args:
#	See op_contain_retbool.c for input args details
#
	.data

	.text
	.extern	op_contain_retbool

ENTRY	opp_contain_retbool
	subq	$8, %rsp		# Bump stack for 16 byte alignment
	CHKSTKALIGN			# Verify stack alignment
	call	op_contain_retbool
	addq	$8, %rsp		# Remove stack alignment bump
	cmpl	$0, %eax
	ret
# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
# This marking is not an issue in Linux but is in Windows Subsystem on Linux (WSL) which does not enable executable stack.
.section        .note.GNU-stack,"",@progbits
