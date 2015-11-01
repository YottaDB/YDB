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
	.title	op_equnul.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_equnul
#	PAGE	+
	.DATA
.extern	undef_inhibit

	.text
.extern	underr

# PUBLIC	op_equnul
ENTRY op_equnul
	mv_if_notdefined %eax, l3
	testw	$mval_m_str,mval_w_mvtype(%eax)
	je	l2
	cmpl	$0,mval_l_strlen(%eax)
	jne	l2
l1:	movl	$1,%eax
	cmpl	$0,%eax
	ret

l2:	movl	$0,%eax
	cmpl	$0,%eax
	ret

l3:	cmpb	$0,undef_inhibit	# not defined
	jne	l1			# if undef_inhibit, then all undefined
					# values are equal to null string
	pushl	%eax			# really undef
	call	underr
	addl	$4,%esp
	ret
# op_equnul ENDP

# END
