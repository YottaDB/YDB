#################################################################
#								#
#	Copyright 2007, 2008 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_forinit.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.INCLUDE	"mval_def.si"

	.sbttl	op_forinit
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	numcmp
.extern	s2n

# PUBLIC	op_forinit
ENTRY op_forinit
	movq	frame_pointer(REG_IP),REG64_SCRATCH1
	movq    REG_PV, msf_ctxt_off(REG64_SCRATCH1)
	popq	msf_mpc_off(REG64_SCRATCH1)
	pushq	REG64_ARG2			#Push args to avoid getting modified across function calls
	pushq	REG64_ARG1
	pushq	REG64_ARG0
	movq	REG64_ARG1,REG64_ACCUM		# 2nd argument
	mv_force_defined REG64_ACCUM, t1
	pushq	REG64_ACCUM
	mv_force_num REG64_ACCUM, t2
	popq	REG64_ACCUM			# restore 2nd argument
	cmpl	$0,mval_l_m1(REG64_ACCUM)
	js	l2
	mv_if_int REG64_ACCUM, l1
	testb 	$mval_esign_mask,mval_b_exp(REG64_ACCUM)
	jne	l2
l1:	movq	0(REG_SP),REG64_ARG0		#compare first with third
	movq	16(REG_SP),REG64_ARG1		#third
	call	numcmp
	addq	$24,REG_SP			#Pop all the argument locally pushed
	movq	frame_pointer(REG_IP),REG64_SCRATCH1
	pushq	msf_mpc_off(REG64_SCRATCH1)
	cmpl	$0,REG32_RET0
	ret

l2:	movq	16(REG_SP),REG64_ARG0		#compare third with first
	movq	0(REG_SP),REG64_ARG1		#first
	call	numcmp
	addq	$24,REG_SP			#Pop all the argument locally pushed
	movq	frame_pointer(REG_IP),REG64_SCRATCH1
	pushq	msf_mpc_off(REG64_SCRATCH1)
	cmpl	$0,REG32_RET0
	ret
# op_forinit ENDP

# END
