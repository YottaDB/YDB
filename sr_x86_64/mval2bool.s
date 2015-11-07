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
# mval2bool.s
#	Convert mval to bool
# arg:
#	%r10 - (aka REG64_RET1) source mval pointer
# return value:
#	condition code is set
#

	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
	.include "debug.si"

	.text
	.extern	s2n

ENTRY	mval2bool
	subq	$8, REG_SP			# Allocate area to align stack to 16 bytes
	CHKSTKALIGN				# Verify stack alignment
	mv_force_defined REG_RET1, isdefined
	movq	REG_RET1, 0(REG_SP)		# Save mval addr across potential call (it may have been changed)
	mv_force_num REG_RET1, skip_conv
	movq    0(REG_SP), REG_RET1		# Restore mval addr
	addq	$8, REG_SP			# Release save area
	cmpl    $0, mval_l_m1(REG_RET1)		# Set condition of flag register
	ret
