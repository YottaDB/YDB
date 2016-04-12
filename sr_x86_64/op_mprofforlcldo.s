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
	.extern	_frame_pointer

	.text
	.extern	_exfun_frame_sp

#
# This is the M profiling version which calls different routine(s) for M profiling purposes.
#
ENTRY	_op_mprofforlcldow
ENTRY	_op_mprofforlcldol
ENTRY	_op_mprofforlcldob
	subq	$8, %rsp				# Bump stack for 16 byte alignment
	CHKSTKALIGN					# Verify stack alignment
	movq	_frame_pointer(%rip), %rdx
	movq	8(%rsp), %rax			# Get our return address
	addq    %rdi, %rax			# Add in return offset parm
        movq	%rax, msf_mpc_off(%rdx)	# Save as return address for this frame
	call	_exfun_frame_sp				# Create new frame
	movq	_frame_pointer(%rip), %rbp # Get updated frame pointer
	movq	msf_temps_ptr_off(%rbp), %r14 # .. and updated temps pointer
	addq	$8, %rsp				# Remove our stack alignment bump
	ret
