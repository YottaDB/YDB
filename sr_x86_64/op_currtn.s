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
	.title	op_currtn.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE "mval_def.si"

	.INCLUDE	"g_msf.si"

	.sbttl	op_currtn
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text

# PUBLIC	op_currtn
ENTRY op_currtn
	movw	$mval_m_str,mval_w_mvtype(REG64_RET1)
	movq	frame_pointer(REG_IP),REG64_SCRATCH1
	movq	msf_rvector_off(REG64_SCRATCH1),REG64_RET0
	movl	mrt_rtn_len(REG64_RET0),REG32_SCRATCH1
	movl	REG32_SCRATCH1,mval_l_strlen(REG64_RET1)
	movq	mrt_rtn_addr(REG64_RET0),REG64_SCRATCH1
	movq	REG64_SCRATCH1,mval_a_straddr(REG64_RET1)
	ret
# op_currtn ENDP

# END
