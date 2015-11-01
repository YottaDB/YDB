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
.extern	mid_len

# PUBLIC	op_currtn
ENTRY op_currtn
	movb	$mval_m_str,mval_b_mvtype(%edx)
	movl	frame_pointer,%eax
	movl	(%eax),%eax
	leal	mrt_rtn_mid(%eax),%eax
	movl	%eax,mval_a_straddr(%edx)
	pushl	%edx			# save mval pointer
	pushl	mval_a_straddr(%edx)
	call	mid_len
	addl	$4,%esp
	popl	%edx			# restore mval pointer
	movl	%eax,mval_l_strlen(%edx)
	ret
# op_currtn ENDP

# END
