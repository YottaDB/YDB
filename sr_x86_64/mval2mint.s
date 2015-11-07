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
# mval2mint.s
#	Convert mval to int
# arg:
#	%r10 - (aka REG64_RET1) source mval pointer
# return value:
#	%eax - return value int
#
	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
	.include "debug.si"

	.text
	.extern	mval2i
	.extern	s2n

ENTRY	mval2mint
	subq	$8, REG_SP			# Allocate area to align stack to 16 bytes
	CHKSTKALIGN				# Verify stack alignment
	mv_force_defined REG64_RET1, isdefined
	movq	REG64_RET1, 0(REG_SP)		# Save mval ptr - mv_force_defined may have re-defined it
        mv_force_num REG64_RET1, skip_conv
	movq	0(REG_SP), REG64_ARG0		# Move saved value to arg reg
	call	mval2i
	addq	$8, REG_SP			# Remove save area
	ret
