#################################################################
#								#
# Copyright (c) 2007-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	#
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
	.extern	exfun_frame

ENTRY	op_forlcldow
ENTRY	op_forlcldol
ENTRY	op_forlcldob
	subq	$8, REG_SP				# Bump stack for 16 byte alignment
	CHKSTKALIGN					# Verify stack alignment
	movq	frame_pointer(REG_IP), REG64_ARG2
	movq	8(REG_SP), REG64_ACCUM			# Get our return address
	addq    REG64_ARG0, REG64_ACCUM			# Add in return offset parm
        movq	REG64_ACCUM, msf_mpc_off(REG64_ARG2)	# Save as return address for this frame
	call	exfun_frame				# Create new frame
	movq	frame_pointer(REG_IP), REG_FRAME_POINTER # Get updated frame pointer
	movq	msf_temps_ptr_off(REG_FRAME_POINTER), REG_FRAME_TMP_PTR # .. and updated temps pointer
	addq	$8, REG_SP				# Remove our stack alignment bump
	ret
# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
# This marking is not an issue in Linux but is in Windows Subsystem on Linux (WSL) which does not enable executable stack.
.section        .note.GNU-stack,"",@progbits
