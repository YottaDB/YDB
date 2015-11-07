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
	.include "mval_def.si"
	.include "g_msf.si"

	.data
	.extern	frame_pointer

	.text

#
# Routine to fill in an mval with the current routine name.
#
# Note since this routine makes no calls, stack alignment is not critical. If ever a call is added then this
# routine should take care to align the stack to 16 bytes and add a CHKSTKALIGN macro.
#
ENTRY	op_currtn
	movw	$mval_m_str, mval_w_mvtype(REG64_RET1)
	movq	frame_pointer(REG_IP), REG64_SCRATCH1
	movq	msf_rvector_off(REG64_SCRATCH1), REG64_RET0
	movl	mrt_rtn_len(REG64_RET0), REG32_SCRATCH1
	movl	REG32_SCRATCH1, mval_l_strlen(REG64_RET1)
	movq	mrt_rtn_addr(REG64_RET0), REG64_SCRATCH1
	movq	REG64_SCRATCH1, mval_a_straddr(REG64_RET1)
	ret
