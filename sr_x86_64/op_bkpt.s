#################################################################
#								#
# Copyright (c) 2007-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

	.include "linkage.si"
	.include "g_msf.si"
	.include "debug.si"

save0	= 0						# Stack offset for first save arg
save1	= 8						# Stack offset for 2nd save arg

	.data
	.extern	frame_pointer
	.extern	zstep_level

	.text
	.extern	gtm_fetch
	.extern	op_retarg
	.extern	op_zbreak
	.extern	op_zst_break
	.extern	op_zst_over
	.extern	op_zstepret
	.extern	opp_ret

ENTRY	opp_zstepret
	subq	$8, REG_SP				# Align stack to 16 bytes
	CHKSTKALIGN					# Verify stack alignment
	movq	frame_pointer(REG_IP), REG64_ACCUM
	movw	msf_typ_off(REG64_ACCUM), REG16_SCRATCH1
	testw	$1, REG16_SCRATCH1
	je	l1
	movq	zstep_level(REG_IP), REG64_ARG2
	cmpq	REG64_ACCUM, REG64_ARG2
	jg	l1
	call	op_zstepret
l1:
	addq	$8, REG_SP				# Remove stack alignment bump
	jmp	opp_ret

ENTRY	opp_zstepretarg
	subq	$24, REG_SP				# Align stack to 16 bytes plus 2 long int save areas
	CHKSTKALIGN					# Verify stack alignment
	movq	REG64_RET0, save0(REG_SP)		# Save input regs
	movq	REG64_RET1, save1(REG_SP)
	movq	frame_pointer(REG_IP), REG64_ACCUM
	movw	msf_typ_off(REG64_ACCUM), REG16_ARG2
	testw	$1, REG16_ARG2
	je	l2
	movq	zstep_level(REG_IP), REG64_ARG2
	cmpq	REG64_ACCUM, REG64_ARG2
	jg	l2
	call	op_zstepret
l2:
	movq	save1(REG_SP), REG64_RET1		# Restore input regs
	movq	save0(REG_SP), REG64_RET0
	addq	$24, REG_SP				# Remove our stack bump
	jmp	op_retarg

ENTRY	op_zbfetch
	movq	frame_pointer(REG_IP), REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)		# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	movb    $0, REG8_ACCUM             		# Variable length argumentt
	call	gtm_fetch
	movq	frame_pointer(REG_IP), REG64_ARG0
	call	op_zbreak
	getframe					# Pushes return addr on stack
	ret

ENTRY	op_zbstart
	movq	frame_pointer(REG_IP), REG64_ARG0
	popq	msf_mpc_off(REG64_ARG0)			# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	call	op_zbreak
	getframe					# Pushes return addr on stack
	ret

ENTRY	op_zstepfetch
	movq	frame_pointer(REG_IP), REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)		# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	movb    $0, REG8_ACCUM             		# Variable length argument
	call	gtm_fetch
	call	op_zst_break
	getframe					# Pushes return addr on stack
	ret

ENTRY	op_zstepstart
	movq	frame_pointer(REG_IP), REG64_ARG2
	popq	msf_mpc_off(REG64_ARG2)			# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	call	op_zst_break
	getframe					# Pushes return addr on stack
	ret

ENTRY	op_zstzbfetch
	movq	frame_pointer(REG_IP), REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)		# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	movb    $0, REG8_ACCUM             		# Variable length argument
	call	gtm_fetch
	movq	frame_pointer(REG_IP), REG64_ARG0
	call	op_zbreak
	call	op_zst_break
	getframe
	ret

ENTRY	op_zstzbstart
	movq	frame_pointer(REG_IP), REG64_ARG0
	popq	msf_mpc_off(REG64_ARG0)			# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	call	op_zbreak
	call	op_zst_break
	getframe					# Pushes return addr on stack
	ret

ENTRY	op_zstzb_fet_over
	movq	frame_pointer(REG_IP), REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)		# Save return address and remove from stack (now 16 byte aligned)
	movb    $0, REG8_ACCUM             		# Variable length argument
	CHKSTKALIGN					# Verify stack alignment
	call	gtm_fetch
	movq	frame_pointer(REG_IP), REG64_ARG0
	call	op_zbreak
	movq	zstep_level(REG_IP), REG64_ARG2
	movq    frame_pointer(REG_IP), REG64_SCRATCH1
	cmpq	REG64_SCRATCH1, REG64_ARG2
	jae	l3
	cmpl	$0, REG32_RET0
	jne	l5
	jmp	l4
l3:
	call	op_zst_break
l4:
	getframe					# Pushes return addr on stack
	ret
l5:
	call	op_zst_over
	movq	frame_pointer(REG_IP), REG64_ARG2
	pushq	msf_mpc_off(REG_IP)			# Restore return address
	ret

ENTRY	op_zstzb_st_over
	movq	frame_pointer(REG_IP), REG64_ARG0
	popq	msf_mpc_off(REG64_ARG0)			# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	call	op_zbreak
	movq	zstep_level(REG_IP), REG64_ARG0
	movq	frame_pointer(REG_IP), REG64_SCRATCH1
	cmpq	REG64_SCRATCH1, REG64_ARG0
	jae	l6
	cmpl	$0, REG32_RET0
	jne	l8
	jmp	l7
l6:
	call	op_zst_break
l7:
	getframe					# Pushes return addr on stack
	ret
l8:
	call	op_zst_over
	movq	frame_pointer(REG_IP), REG64_ARG0
	pushq	msf_mpc_off(REG64_ARG0)			# Restore return address
	ret

ENTRY	op_zst_fet_over
	movq	frame_pointer(REG_IP), REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)		# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	movb    $0, REG8_ACCUM				# Variable length argument
	call	gtm_fetch
	movq	zstep_level(REG_IP), REG64_ACCUM
	movq	frame_pointer(REG_IP), REG64_SCRATCH1
	cmpq	REG64_SCRATCH1, REG64_ACCUM
	jg	l9
	call	op_zst_break
	getframe					# Pushes return addr on stack
	ret
l9:
	call	op_zst_over
	movq	frame_pointer(REG_IP), REG64_ACCUM
	pushq	msf_mpc_off(REG64_ACCUM)		# Restore return address
	ret

ENTRY	op_zst_st_over
	movq	frame_pointer(REG_IP), REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)		# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	movq	zstep_level(REG_IP), REG64_ARG2
	cmpq	REG64_ACCUM, REG64_ARG2
	jg	l10
	call	op_zst_break
	getframe					# Pushes return addr on stack
	ret
l10:
	call	op_zst_over
	movq	frame_pointer(REG_IP), REG64_ARG2
	pushq	msf_mpc_off(REG64_ARG2)			# Restore return address
	ret

ENTRY	opp_zst_over_ret
	subq	$8, REG_SP				# Align stack to 16 bytes
	CHKSTKALIGN					# Verify stack alignment
	movq	frame_pointer(REG_IP), REG64_ACCUM
	movw	msf_typ_off(REG64_ACCUM), REG16_ARG2
	testw	$1, REG16_ARG2
	je	l11
	movq	zstep_level(REG_IP), REG64_ARG2
	movq	msf_old_frame_off(REG64_ACCUM), REG64_ACCUM
	cmpq	REG64_ACCUM, REG64_ARG2
	jg	l11
	call	op_zstepret
l11:
	addq	$8, REG_SP				# Remove stack alignment bump
	jmp	opp_ret

ENTRY	opp_zst_over_retarg
	subq	$24, REG_SP				# Align stack to 16 bytes plus 2 long int save areas
	CHKSTKALIGN					# Verify stack alignment
	movq	REG64_RET0, save0(REG_SP)		# Save input regs
	movq	REG64_RET1, save1(REG_SP)
	movq	frame_pointer(REG_IP), REG64_ACCUM
	movw	msf_typ_off(REG64_ACCUM), REG16_ARG2
	testw	$1, REG16_ARG2
	je	l12
	movq	zstep_level(REG_IP), REG64_ARG2
	movq	msf_old_frame_off(REG64_ACCUM), REG64_ACCUM
	cmpq	REG64_ACCUM, REG64_ARG2
	jg	l12
	call	op_zstepret
l12:
	movq	save1(REG_SP), REG64_RET1		# Restore input regs
	movq	save0(REG_SP), REG64_RET0
	addq	$24, REG_SP				# Remove our stack bump
	jmp	op_retarg
