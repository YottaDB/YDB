#################################################################
#								#
# Copyright (c) 2001-2015 Fidelity National Information 	#
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

	.data
	.extern	dollar_truth
	.extern	frame_pointer

	.text
	.extern	exfun_frame
	.extern	push_tval

	#
	# Note this routine calls exfun_frame() instead of copy_stack_frame() because this routine needs to provide a
	# separate set of compiler temps for use by the new frame. Particularly when it called on same line with FOR.
	#

ENTRY op_callspb
	movl	frame_pointer,%edx
	movl	(%esp),%eax
	movl	%eax,msf_mpc_off(%edx)
	addl	$2,msf_mpc_off(%edx)	# store pc in MUMPS stack frame
doit:	call	exfun_frame
	pushl	dollar_truth
	call	push_tval
	addl	$4,%esp
	movl	frame_pointer,%edx
	movl	msf_temps_ptr_off(%edx),%edi
	ret

ENTRY op_callspw
ENTRY op_callspl
	movl	frame_pointer,%edx
	movl	(%esp),%eax
	movl	%eax,msf_mpc_off(%edx)
	addl	$5,msf_mpc_off(%edx)	# store pc in MUMPS stack frame
	jmp	doit
