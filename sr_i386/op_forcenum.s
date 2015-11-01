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
	.title	op_forcenum.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_forcenum
#	PAGE	+
	.text
.extern	s2n

#	edx - source mval
#	eax - destination mval

# PUBLIC	op_forcenum
ENTRY op_forcenum
	testb	$mval_m_nm,mval_b_mvtype(%edx)
	jne	l10
	pushl	%eax
	pushl	%edx
	call	s2n
	popl	%edx
	popl	%eax
l10:	testb	$mval_m_str,mval_b_mvtype(%edx)
	je	l20
	testb	$mval_m_num_approx,mval_b_mvtype(%edx)
	je	l40
l20:	testb	$mval_m_int_without_nm,mval_b_mvtype(%edx)
	je	l30
	movb	$mval_m_int,mval_b_mvtype(%eax)
	movl	mval_l_m1(%edx),%edx
	movl	%edx,mval_l_m1(%eax)
	ret

l30:	pushl	%ebx
	movb	$mval_m_nm,mval_b_mvtype(%eax)
	movb	mval_b_exp(%edx),%bl
	movb	%bl,mval_b_exp(%eax)

#	Copy the only numeric part of Mval from [edx] to [eax].

	movl	mval_l_m0(%edx),%ebx
	movl	%ebx,mval_l_m0(%eax)
	movl	mval_l_m1(%edx),%ebx
	movl	%ebx,mval_l_m1(%eax)
	popl	%ebx
	ret

l40:
#	Copy the Mval from [edx] to [eax].

	pushl	%edi
	pushl	%esi
	movl	%eax,%edi
	movl	%edx,%esi
	movl	$mval_byte_len,%ecx
	REP
	movsb
	popl	%esi
	popl	%edi
	ret
# op_forcenum ENDP

# END
