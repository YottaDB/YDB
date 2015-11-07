#################################################################
#								#
# Copyright (c) 2007-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
#
# mint2mval.s
#	Convert int to mval
# args:
#	%rax   - (aka REG64_RET0) - Destination mval pointer
#	%r10d  - (aka REG32_RET1) - Input integer value to convert
#
	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
	.include "debug.si"

	.text
	.extern	i2mval

ENTRY	mint2mval
	subq	$8, REG_SP		# Align stack to 16 bytes
	CHKSTKALIGN			# Verify stack alignment
	movl	REG32_RET1, REG32_ARG1
        movq	REG64_RET0, REG64_ARG0
	call	i2mval
	addq	$8, REG_SP
	ret
