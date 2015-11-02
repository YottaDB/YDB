#################################################################
#								#
#	Copyright 2007, 2010 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_extjmp.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"

	.sbttl	op_extjmp
#	PAGE	+
	.DATA
.extern	ERR_GTMCHECK
.extern	ERR_LABELUNKNOWN
.extern	frame_pointer

	.text
.extern	auto_zlink
.extern	flush_jmp
.extern	rts_error

# PUBLIC	op_extjmp
ENTRY op_extjmp
	putframe
	addq	$8,REG_SP        # burn return pc
	cmpq	$0,REG64_ARG0
	je	l2
	cmpq	$0,REG64_ARG1
	je	l4

l1:	#&(code_offset) for this label (usually & of linenumber table entry
	movq	(REG64_ARG1),REG64_ACCUM

	#check if label has been replaced and error out if yes.
	cmpq	$0,REG64_ACCUM
	je	l4

	#Offset to label
	movslq	0(REG64_ACCUM),REG64_ACCUM

	#code base reg
	movq	mrt_ptext_adr(REG64_ARG0),REG64_ARG1
	#transfer address: codebase reg + offset to label
	addq	REG64_ARG1,REG64_ACCUM
	movq	REG64_ACCUM,REG64_ARG2

	#linkage ptr(ctxt ptr)
	movq	mrt_lnk_ptr(REG64_ARG0),REG64_ARG1

	call	flush_jmp
	getframe
	ret

l2:	#point to "temp" arg auto_zlink can set with new labaddr
	subq	$8,REG_SP
	movq	REG_SP, REG64_ARG1

	movq	frame_pointer(REG_IP),REG64_ACCUM
	movq	msf_mpc_off(REG64_ACCUM),REG64_ARG0
	call	auto_zlink

	cmpq	$0,REG64_RET0
	je	l3
	movq	REG64_RET0,REG64_ARG0

	popq	REG64_ARG1
	cmpq	$0,REG64_ARG1
	jne	l1
	jmp	l4

l3:	movl	ERR_GTMCHECK(REG_IP),REG32_ARG1
	movl	$1,REG32_ARG0
	movb    $0,REG8_ACCUM             # variable length argument
	call	rts_error
	getframe
	ret

l4:	movl	ERR_LABELUNKNOWN(REG_IP),REG32_ARG1
	movl	$1,REG32_ARG0
	movb    $0,REG8_ACCUM             # variable length argument
	call	rts_error
	getframe
	ret
# op_extjmp ENDP

# END
