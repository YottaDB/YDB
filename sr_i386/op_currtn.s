#################################################################
#								#
#	Copyright 2001, 2006 Fidelity Information Services, Inc	#
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
	movw	$mval_m_str,mval_w_mvtype(%edx)
	movl	frame_pointer,%eax
	movl	msf_rvector_off(%eax),%eax
	pushl	mrt_rtn_len(%eax)
	popl	mval_l_strlen(%edx)		# %edx->str.len = frame_pointer->rvector->routine_name.len
	movl	mrt_rtn_addr(%eax),%eax
	movl	%eax,mval_a_straddr(%edx)	# %edx->str.addr = frame_pointer->rvector->routine_name.addr
	ret
# op_currtn ENDP

# END
