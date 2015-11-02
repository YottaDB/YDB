#################################################################
#								#
#	Copyright 2007, 2011 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_mproflinestart.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_mproflinestart
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	pcurrpos

# PUBLIC	op_mproflinestart
ENTRY op_mproflinestart
	movq	frame_pointer(REG_IP),REG64_RET1
	movq    (REG_SP),REG64_ACCUM
	movq	REG64_ACCUM,msf_mpc_off(REG64_RET1)
	movq    REG_PV, msf_ctxt_off(REG64_RET1)      # Save ctxt into frame_pointer
	call	pcurrpos
	ret
# op_mproflinestart ENDP

# END
