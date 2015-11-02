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
	.title	op_linestart.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_linestart
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
# PUBLIC	op_linestart
ENTRY op_linestart
	movq    frame_pointer(REG_IP),REG64_RET1
        movq    (REG_SP),REG64_ACCUM
        movq    REG64_ACCUM,msf_mpc_off(REG64_RET1)   # save incoming return address in frame_pointer->mpc
	movq    REG_PV, msf_ctxt_off(REG64_RET1)      # Save ctxt in frame_pointer
	ret
# op_linestart ENDP

# END
