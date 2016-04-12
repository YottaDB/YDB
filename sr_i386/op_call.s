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
	.title	op_call.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_callb
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	copy_stack_frame

# PUBLIC	op_callb
ENTRY op_callb
	movl	frame_pointer,%edx
	movl	(%esp),%eax
	movl	%eax,msf_mpc_off(%edx)
	addl	$2,msf_mpc_off(%edx)
	call	copy_stack_frame
	ret
# op_callb ENDP

	.sbttl	op_callw, op_calll
# PUBLIC	op_callw, op_calll
ENTRY op_calll
ENTRY op_callw
	movl	frame_pointer,%edx
	movl	(%esp),%eax
	movl	%eax,msf_mpc_off(%edx)
	addl	$5,msf_mpc_off(%edx)
	call	copy_stack_frame
	ret
# op_calll ENDP

# END
