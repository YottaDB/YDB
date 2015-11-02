#################################################################
#								#
#	Copyright 2001, 2008 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_neg.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_neg
#	PAGE	+
	.text
#
# op_neg ( mval *u, mval *v ) : u = -v
#
#	edx - source mval      = &v
#	eax - destination mval = &u

.extern	s2n
.extern underr

# PUBLIC	op_neg
ENTRY op_neg
	pushl	%eax
	mv_force_defined %edx, isdefined
	popl	%eax
	mv_if_number %edx, numer
	pushl	%eax
	pushl	%edx
	call	s2n
	popl	%edx
	popl	%eax
numer:	mv_if_notint %edx, float
	movw	$mval_m_int,mval_w_mvtype(%eax)
	movl	mval_l_m1(%edx),%edx
	negl	%edx
	movl	%edx,mval_l_m1(%eax)
	ret

float:	pushl	%ebx		# need a temp register
	movw	$mval_m_nm,mval_w_mvtype(%eax)
	movb	mval_b_exp(%edx),%bl
	xorb	$mval_esign_mask,%bl		# flip the sign bit
	movb	%bl,mval_b_exp(%eax)
	movl	mval_l_m0(%edx),%ebx
	movl	%ebx,mval_l_m0(%eax)
	movl	mval_l_m1(%edx),%ebx
	movl	%ebx,mval_l_m1(%eax)
	popl	%ebx
	ret
# op_neg	ENDP

#END
