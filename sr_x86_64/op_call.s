#################################################################
#								#
#	Copyright 2007 Fidelity Information Services, Inc	#
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
ENTRY op_calll
ENTRY op_callw
ENTRY op_callb
	movq	frame_pointer(REG_IP),REG64_SCRATCH1
	movq	(REG_SP),REG64_ACCUM
	enter $0,$0
	movq	REG64_ACCUM,msf_mpc_off(REG64_SCRATCH1)
	addq	REG64_ARG0,msf_mpc_off(REG64_SCRATCH1)	# OCNT_REF triple newly added to send byte offset from return address
	call	copy_stack_frame		# Refer emit_code.c
	leave
	ret
# op_callb ENDP
# END
