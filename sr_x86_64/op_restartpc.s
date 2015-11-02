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
	.title	op_restartpc.s
	.sbttl	op_restartpc

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
.include "g_msf.si"

	.DATA
.extern	restart_pc
.extern restart_ctxt
.extern frame_pointer

	.text
# PUBLIC	op_restartpc
ENTRY op_restartpc
	movq	(REG_SP),REG64_ACCUM
	subq	$6,REG64_ACCUM 		# xfer call size is constant
	movq	REG64_ACCUM,restart_pc(REG_IP)
	movq	frame_pointer(REG_IP),REG64_ACCUM
	movq	msf_ctxt_off(REG64_ACCUM),REG64_SCRATCH1
	movq	REG64_SCRATCH1,restart_ctxt(REG_IP)
	ret
# op_restartpc ENDP

# END
