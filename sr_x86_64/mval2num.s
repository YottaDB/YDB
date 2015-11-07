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
# mval2num.s
#	Force mval to numeric value if not already - if is approximate, also force it to string
# arg:
#	%r10 - (aka REG64_RET1) source mval pointer
#
# Note updates mval in place
#
	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
	.include "debug.si"

	.text
	.extern	n2s
	.extern	s2n

ENTRY	mval2num
	subq	$8, REG_SP				# Allocate area to align stack to 16 bytes
	CHKSTKALIGN					# Verify stack alignment
	mv_force_defined REG64_RET1, isdefined
	movq	REG64_RET1, 0(REG_SP)			# Save mval ptr - mv_force_defined may have re-defined it
        mv_force_num REG64_RET1, l1
	movq	0(REG_SP), REG64_RET1			# Restore saved mval pointer
        mv_force_str_if_num_approx REG64_RET1, l2
	addq	$8, REG_SP				# Remove save area
	ret
