#################################################################
#								#
#	Copyright 2001 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_mprofcall.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_mprofcallb
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	copy_stack_frame_sp

# PUBLIC	op_mprofcallb
ENTRY op_mprofcallb
	movl	frame_pointer,%edx
	movl	(%esp),%eax
	movl	%eax,msf_mpc_off(%edx)
	addl	$2,msf_mpc_off(%edx)
	call	copy_stack_frame_sp
	ret
# op_callb ENDP

	.sbttl	op_mprofcallw, op_mprofcalll
# PUBLIC	op_mprofcallw, op_mprofcalll
ENTRY op_mprofcalll
ENTRY op_mprofcallw
	movl	frame_pointer,%edx
	movl	(%esp),%eax
	movl	%eax,msf_mpc_off(%edx)
	addl	$5,msf_mpc_off(%edx)
	call	copy_stack_frame_sp
	ret
# op_mprofcalll ENDP

# END
