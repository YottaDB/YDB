#################################################################
#								#
#	Copyright 2007, 2009 Fidelity Information Services, Inc       #
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_linefetch.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_linefetch
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	gtm_fetch

# PUBLIC	op_linefetch
ENTRY op_linefetch
	movq	frame_pointer(REG_IP),REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)            # save incoming return PC in frame_pointer->mpc
	movq	REG_PV, msf_ctxt_off(REG64_ACCUM)   # Save linkage pointer
	movb    $0,REG8_ACCUM                       # variable length argument
	call	gtm_fetch
	movq	frame_pointer(REG_IP),REG64_ACCUM
	pushq	msf_mpc_off(REG64_ACCUM)
	ret
# op_linefetch ENDP

# END
