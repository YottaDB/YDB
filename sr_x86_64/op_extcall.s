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
	.title	op_extcall.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_extcall
#	PAGE	+
	.DATA
.extern	ERR_GTMCHECK
.extern	ERR_LABELUNKNOWN
.extern	frame_pointer

	.text
.extern	auto_zlink
.extern	new_stack_frame
.extern	rts_error

# PUBLIC	op_extcall
ENTRY op_extcall
	putframe
	addq    $8,REG_SP     					# burn the saved return pc
	cmpq	$0,REG64_ARG0					# if rhdaddr == 0, not yet linked into image
	je	l2
	cmpq    $0,REG64_ARG1					# if labaddr == 0 && rhdaddr != 0, label does not exist
        je      l4

l1:	movq	(REG64_ARG1),REG64_ACCUM			# &(code_offset) for this label (usually & of lntabent)
	cmpq	$0,REG64_ACCUM
	je	l4
	movslq	0(REG64_ACCUM),REG64_ACCUM			# code offset for this label
	movq	mrt_ptext_adr(REG64_ARG0),REG64_ARG1
	addq	REG64_ARG1,REG64_ACCUM
	movq	REG64_ACCUM,REG64_ARG2
	movq	mrt_lnk_ptr(REG64_ARG0),REG64_ARG1
	call	new_stack_frame
	getframe
	ret

l2:	cmpq	$0,REG64_ARG1
	jne	l4
	subq    $8,REG_SP					# Pass the SP as 2nd argument to auto_zlink.
        movq    REG_SP,REG64_ARG1				# auto_zlink will populate this with labaddr
        movq    frame_pointer(REG_IP),REG64_ACCUM
        movq    msf_mpc_off(REG64_ACCUM),REG64_ARG0
	call	auto_zlink
	cmpq	$0,REG64_RET0
	je	l3
	movq	REG64_RET0,REG64_ARG0
	popq	REG64_ARG1					# Get the 2nd argument populated by auto_zlink
	cmpq	$0,REG64_ARG1
	jne	l1

l3:	movl	ERR_GTMCHECK(REG_IP),REG32_ARG1
	movl	$1,REG32_ARG0
	movb    $0,REG8_ACCUM             # variable length argument
	call	rts_error
	getframe
	ret

l4:	movl    ERR_LABELUNKNOWN(REG_IP),REG32_ARG1
        movl    $1,REG32_ARG0
	movb    $0,REG8_ACCUM             # variable length argument
	call	rts_error
	getframe
	ret
# op_extcall ENDP

# END
