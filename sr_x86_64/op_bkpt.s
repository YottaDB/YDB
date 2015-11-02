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
	.title	op_bkpt.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE "g_msf.si"

	.sbttl	opp_zstepret
#	PAGE	+
	.DATA
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

# PUBLIC	opp_zstepret
ENTRY opp_zstepret
	movq	frame_pointer(REG_IP),REG64_ACCUM
	movw	msf_typ_off(REG64_ACCUM),REG16_SCRATCH1
	testw	$1,REG16_SCRATCH1
	je	l1
	movq	zstep_level(REG_IP),REG64_ARG2
	cmpq	REG64_ACCUM,REG64_ARG2
	jg	l1
	call	op_zstepret
l1:	jmp	opp_ret

# PUBLIC	opp_zstepretarg
ENTRY opp_zstepretarg
	pushq   REG64_RET0
	pushq	REG64_RET1
	movq	frame_pointer(REG_IP),REG64_ACCUM
	movw	msf_typ_off(REG64_ACCUM),REG16_ARG2
	testw	$1,REG16_ARG2
	je	l2
	movq	zstep_level(REG_IP),REG64_ARG2
	cmpq	REG64_ACCUM, REG64_ARG2
	jg	l2
	call	op_zstepret
l2:	popq	REG64_RET1
	popq	REG64_RET0
	jmp	op_retarg
# opp_zstepretarg ENDP

# PUBLIC	op_zbfetch
ENTRY op_zbfetch
	movq	frame_pointer(REG_IP),REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)
	movb    $0,REG8_ACCUM             # variable length argumentt
	call	gtm_fetch
	#popq	REG64_ACCUM
	#leaq	(REG_SP,REG64_ACCUM,8),REG_SP
	movq	frame_pointer(REG_IP),REG64_ARG0
	call	op_zbreak
	getframe
	ret
# op_zbfetch ENDP

# PUBLIC	op_zbstart
ENTRY op_zbstart
	movq	frame_pointer(REG_IP),REG64_ARG0
	popq	msf_mpc_off(REG64_ARG0)
	call	op_zbreak
	getframe
	ret
# op_zbstart ENDP

# PUBLIC	op_zstepfetch
ENTRY op_zstepfetch
	movq	frame_pointer(REG_IP),REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)
	movb    $0,REG8_ACCUM             # variable length argument
	call	gtm_fetch
	#popq	REG64_ACCUM
	#leaq	(REG_SP,REG64_ACCUM,8),REG_SP
	call	op_zst_break
	getframe
	ret
# op_zstepfetch ENDP

# PUBLIC	op_zstepstart
ENTRY op_zstepstart
	movq	frame_pointer(REG_IP),REG64_ARG2
	popq	msf_mpc_off(REG64_ARG2)
	call	op_zst_break
	getframe
	ret
# op_zstepstart ENDP

# PUBLIC	op_zstzbfetch
ENTRY op_zstzbfetch
	movq	frame_pointer(REG_IP),REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)
	movb    $0,REG8_ACCUM             # variable length argument
	call	gtm_fetch
	#popq	REG64_ACCUM
	#leaq	(REG_SP,REG64_ACCUM,8),REG_SP
	movq	frame_pointer(REG_IP),REG64_ARG0
	call	op_zbreak
	call	op_zst_break
	getframe
	ret
# op_zstzbfetch ENDP

# PUBLIC	op_zstzbstart
ENTRY op_zstzbstart
	movq	frame_pointer(REG_IP),REG64_ARG0
	popq	msf_mpc_off(REG64_ARG0)
	call	op_zbreak
	call	op_zst_break
	getframe
	ret
# op_zstzbstart ENDP

# PUBLIC	op_zstzb_fet_over
ENTRY op_zstzb_fet_over
	movq	frame_pointer(REG_IP),REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)
	movb    $0,REG8_ACCUM             # variable length argument
	call	gtm_fetch
	#popq	REG64_ACCUM
	#leaq	(REG_SP,REG64_ACCUM,8),REG_SP
	movq	frame_pointer(REG_IP),REG64_ARG0
	call	op_zbreak
	movq	zstep_level(REG_IP),REG64_ARG2
	movq    frame_pointer(REG_IP), REG64_SCRATCH1
	cmpq	REG64_SCRATCH1,REG64_ARG2
	jae	l3
	cmpl	$0,REG32_RET0
	jne	l5
	jmp	l4

l3:	call	op_zst_break
l4:	getframe
	ret

l5:	call	op_zst_over
	movq	frame_pointer(REG_IP),REG64_ARG2
	pushq	msf_mpc_off(REG_IP)
	ret
# op_zstzb_fet_over ENDP

# PUBLIC	op_zstzb_st_over
ENTRY op_zstzb_st_over
	movq	frame_pointer(REG_IP),REG64_ARG0
	popq	msf_mpc_off(REG64_ARG0)
	call	op_zbreak
	movq	zstep_level(REG_IP),REG64_ARG0
	movq	frame_pointer(REG_IP),REG64_SCRATCH1
	cmpq	REG64_SCRATCH1,REG64_ARG0
	jae	l6
	cmpl	$0,REG32_RET0
	jne	l8
	jmp	l7

l6:	call	op_zst_break
l7:	getframe
	ret

l8:	call	op_zst_over
	movq	frame_pointer(REG_IP),REG64_ARG0
	pushq	msf_mpc_off(REG64_ARG0)
	ret
# op_zstzb_st_over ENDP

# PUBLIC	op_zst_fet_over
ENTRY op_zst_fet_over
	movq	frame_pointer(REG_IP),REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)
	movb    $0,REG8_ACCUM             # variable length argument
	call	gtm_fetch
	#popq	REG64_ACCUM
	#leaq	(REG_SP,REG64_ACCUM,8),REG_SP
	movq	zstep_level(REG_IP),REG64_ACCUM
	movq	frame_pointer(REG_IP),REG64_SCRATCH1
	cmpq	REG64_SCRATCH1,REG64_ACCUM
	jg	l9
	call	op_zst_break
	getframe
	ret

l9:	call	op_zst_over
	movq	frame_pointer(REG_IP),REG64_ACCUM
	pushq	msf_mpc_off(REG64_ACCUM)
	ret
# op_zst_fet_over ENDP

# PUBLIC	op_zst_st_over
ENTRY op_zst_st_over
	movq	frame_pointer(REG_IP),REG64_ACCUM
	popq	msf_mpc_off(REG64_ACCUM)
	movq	zstep_level(REG_IP),REG64_ARG2
	cmpq	REG64_ACCUM,REG64_ARG2
	jg	l10
	call	op_zst_break
	getframe
	ret

l10:	call	op_zst_over
	movq	frame_pointer(REG_IP),REG64_ARG2
	pushq	msf_mpc_off(REG64_ARG2)
	ret
# op_zst_st_over ENDP

# PUBLIC	opp_zst_over_ret
ENTRY opp_zst_over_ret
	movq	frame_pointer(REG_IP),REG64_ACCUM
	movw	msf_typ_off(REG64_ACCUM),REG16_ARG2
	testw	$1,REG16_ARG2
	je	l11
	movq	zstep_level(REG_IP),REG64_ARG2
	movq	msf_old_frame_off(REG64_ACCUM),REG64_ACCUM
	cmpq	REG64_ACCUM,REG64_ARG2
	jg	l11
	call	op_zstepret
l11:	jmp	opp_ret
# opp_zst_over_ret ENDP

# PUBLIC	opp_zst_over_retarg
ENTRY opp_zst_over_retarg
	pushq	REG64_RET0
	pushq	REG64_RET1
	movq	frame_pointer(REG_IP),REG64_ACCUM
	movw	msf_typ_off(REG64_ACCUM),REG16_ARG2
	testw	$1,REG16_ARG2
	je	l12
	movq	zstep_level(REG_IP),REG64_ARG2
	movq	msf_old_frame_off(REG64_ACCUM),REG64_ACCUM
	cmpq	REG64_ACCUM,REG64_ARG2
	jg	l12
	call	op_zstepret
l12:	popq	REG64_RET1
	popq	REG64_RET0
	jmp	op_retarg
# opp_zst_over_retarg ENDP

# END
