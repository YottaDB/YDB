#################################################################
#								#
# Copyright (c) 2007-2026 Fidelity National Information		#
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
	movq	(REG64_RET1), REG64_OUT_ARG0 # Move first qword of source mval to 64bit register rdi
	andq	$~mval_m_aliascont, REG64_OUT_ARG0	# Don't propagate alias container flag. Only works if mval_w_mvtype == 0
	movq	8(REG64_RET1), REG64_OUT_ARG1 # Move second qword of source mval to rsi.
	movq	16(REG64_RET1), REG64_OUT_ARG2 # Move third qword from source mval into register
	movq	24(REG64_RET1), REG64_OUT_ARG3 # Move fourth qword from source mval into register
	movq	REG64_OUT_ARG0, (REG64_RET0) # Move first qword (less alias bit) to dest mval
	movq	REG64_OUT_ARG1, 8(REG64_RET0) # Move second qword to dest mval
	movq	REG64_OUT_ARG2, 16(REG64_RET0) # Move third qword to dest mval
	movq	REG64_OUT_ARG3, 24(REG64_RET0) # Move fourth qword to dest mval
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
