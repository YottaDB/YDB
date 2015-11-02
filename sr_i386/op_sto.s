#################################################################
#								#
#	Copyright 2001, 2009 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_sto.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_sto
#	PAGE	+
	.DATA
.extern	literal_null
.extern	undef_inhibit

	.text
.extern	underr

# PUBLIC	op_sto
ENTRY op_sto
	mv_if_notdefined %edx, b
a:	pushl	%edi
	pushl	%esi
	movl	$mval_byte_len,%ecx
	movl	%edx,%esi
	movl	%eax,%edi
	REP
	movsb
	andw	$~mval_m_aliascont, mval_w_mvtype(%eax)	# Don't propagate alias container flag
	popl	%esi
	popl	%edi
	ret

b:	cmpb	$0,undef_inhibit
	je	clab
	leal	literal_null,%edx
	jmp	a

clab:	pushl	%edx
	call	underr
	addl	$4,%esp
	ret

# op_sto	ENDP

# END
