#################################################################
#								#
# Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
# This module is derived from FIS code.
#################################################################
#
# op_mval2bool.s
#	Convert mval to bool.
# args:
#	See mval2bool.c for input args details
#
	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.text
	.extern	mval2bool

ENTRY	op_mval2bool
	subq	$8, %rsp	# Allocate area to align stack to 16 bytes
	CHKSTKALIGN		# Verify stack alignment
	call	mval2bool	# Call C function `mval2bool` with the same parameters that we were
				#	passed in with. This does the bulk of the needed $ZYSQLNULL processing.
				#	for boolean expression evaluation. The `bool_result` return value
				#	would be placed in `%rax`.
	addq	$8, %rsp	# Undo stack alignment allocation
	cmpl    $0, %eax	# Set condition of flag register
	ret

# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
# This marking is not an issue in Linux but is in Windows Subsystem on Linux (WSL) which does not enable executable stack.
.section        .note.GNU-stack,"",@progbits
