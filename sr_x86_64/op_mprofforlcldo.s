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

	.include "linkage.si"
	.include "g_msf.si"
	.include "debug.si"

	.data
	.extern	frame_pointer

	.text
	.extern	exfun_frame_sp

#
# This is the M profiling version which calls different routine(s) for M profiling purposes.
#
ENTRY	op_mprofforlcldow
ENTRY	op_mprofforlcldol
ENTRY	op_mprofforlcldob
	subq	$8, REG_SP				# Bump stack for 16 byte alignment
	CHKSTKALIGN					# Verify stack alignment
	movq	frame_pointer(REG_IP), REG64_ARG2
	movq	8(REG_SP), REG64_ACCUM			# Get our return address
	addq    REG64_ARG0, REG64_ACCUM			# Add in return offset parm
        movq	REG64_ACCUM, msf_mpc_off(REG64_ARG2)	# Save as return address for this frame
	call	exfun_frame_sp				# Create new frame
	movq	frame_pointer(REG_IP), REG_FRAME_POINTER # Get updated frame pointer
	movq	msf_temps_ptr_off(REG_FRAME_POINTER), REG_FRAME_TMP_PTR # .. and updated temps pointer
	addq	$8, REG_SP				# Remove our stack alignment bump
	ret
