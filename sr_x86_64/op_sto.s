#################################################################
#								#
# Copyright (c) 2007-2016 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
	.include "debug.si"

	.data
	.extern	literal_null
	.extern	undef_inhibit

	.text
	.extern	underr

ENTRY	op_sto
	subq	$8, REG_SP					# Bump stack for 16 byte alignment
	CHKSTKALIGN						# Verify stack alignment
	mv_if_notdefined REG64_RET1, notdef
nowdef:
	movl	$mval_qword_len, REG32_ARG3
	movq	REG64_RET1, REG64_ARG1
	movq	REG64_RET0, REG64_ARG0
	REP
	movsq
	andw	$~mval_m_aliascont, mval_w_mvtype(REG64_RET0)	# Don't propagate alias container flag
done:
	addq	$8, REG_SP					# Remove stack alignment bump
	ret
notdef:
	cmpb	$0, undef_inhibit(REG_IP)
	je	clab
	leaq	literal_null(REG_IP), REG_RET1
	jmp	nowdef
clab:
	movq	REG_RET1, REG64_ARG0
	movb    $0, REG8_ACCUM             			# Variable length argument
	call	underr
	jmp	done
