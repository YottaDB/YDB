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
ENTRY op_mprofcallw
ENTRY op_mprofcalll
	movq    frame_pointer(REG_IP),REG64_SCRATCH1
        movq    (REG_SP),REG64_ACCUM
        movq    REG64_ACCUM,msf_mpc_off(REG64_SCRATCH1)
        addq    REG64_ARG0,msf_mpc_off(REG64_SCRATCH1) # OCNT_REF triple newly added to send byte offset from return address
	call	copy_stack_frame_sp			# Refer emit_code.c
	ret
# op_callb ENDP
# END
