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
#	%r10 - (aka %r10) source mval pointer
# return value:
#	%eax - return value int
#
	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
	.include "debug.si"

	.text
	.extern	_mval2i
	.extern	_s2n
	.extern	_underr
	.extern	_underr_strict

ENTRY	_mval2mint
	subq	$8, %rsp			# Allocate area to align stack to 16 bytes
	CHKSTKALIGN				# Verify stack alignment
	mv_force_defined %r10, isdefined
	movq	%r10, 0(%rsp)		# Save mval ptr - mv_force_defined may have re-defined it
        mv_force_num %r10, skip_conv
	movq	0(%rsp), %rdi		# Move saved value to arg reg
	call	_mval2i
	addq	$8, %rsp			# Remove save area
	ret
