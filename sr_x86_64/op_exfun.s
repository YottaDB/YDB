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

	.include "g_msf.si"
	.include "linkage.si"
	.include "debug.si"
#
# Routine to set up the stack frame for a local (same routine) invocation. It can
# be one of any of the following forms:
#
#   1. A local call with parameters (OC_EXCAL triple - DO rtn(parms)). In this case the
#      address of the return value is NULL.
#   2. A local extrinsic with no parameters (OC_EXFUN triple $$func).
#   2. A local extrinsic with parameters (OC_EXFUN triple $$func(parms)).
#
# Arguments:
#
#   REG_ARG0: Address of return value
#   REG_ARG1: Offset from our return address to where this stackframe needs to return
#   REG_ARG2: Mask
#   REG_ARG3: Actualcnt
#   REG_ARG4: Actuallist1 address
#   REG_ARG5: Actuallist2 address
#   (remainder of args on stack if any)
#
# Note no need to save %rbp in the prologue as it gets reset to the new frame when we return
#

	.data
	.extern	ERR_GTMCHECK
	.extern	dollar_truth
	.extern	frame_pointer

	.text
	.extern	exfun_frame
	.extern	push_parm
	.extern	rts_error

arg2_off	= -48
arg1_off	= -40
arg0_off	= -32
act_cnt		= -24
mask_arg	= -20
ret_val		= -16
rtn_pc		= -8
FRAME_SAVE_SIZE	= 56					# This size 16 byte aligns the stack

ENTRY	op_exfun
	movq	0(REG_SP), REG64_SCRATCH1		# Save return address for storage in M stack frame
	movq	REG_SP, %rbp				# Copy previous stack-frame to %rbp
	subq	$FRAME_SAVE_SIZE, REG_SP		# Create save area
	CHKSTKALIGN					# Verify stack alignment
	movq	REG64_SCRATCH1, rtn_pc(%rbp) 		# Save return address
	movq	REG64_ARG0, ret_val(%rbp)		# Save incoming arguments
	movl	REG32_ARG2, mask_arg(%rbp)
	movl	REG32_ARG3, act_cnt(%rbp)
	movq	REG64_ARG4, arg0_off(%rbp)
	movq	REG64_ARG5, arg1_off(%rbp)
	movq	frame_pointer(REG_IP), REG64_ARG2
	movq	rtn_pc(%rbp), REG64_ACCUM		# Verify the immediate instruction after
	cmpb	$JMP_Jv, 0(REG64_ACCUM)			# .. this function call
	jne	error
	movq	REG64_ACCUM, msf_mpc_off(REG64_ARG2)
	addq	REG64_ARG1, msf_mpc_off(REG64_ARG2)
	call	exfun_frame
	movl	act_cnt(%rbp), REG32_ACCUM
	cmpl	$0, REG32_ACCUM                         # arg0, arg1, arg2 are stored in save area off of %rbp
	je	no_arg
	cmpl	$1, REG32_ACCUM				# We have only one register free for push_parm args
	je	arg0
	#
	# We have more than 1 actuallist parameters so we need some aligned space on the stack for parameters that
	# don't fit in the 6 parm registers. Only the first actuallist parameter can fit in a parm register. All
	# others must reside on the stack starting at the lowest address. So for example, if we need 7 slots, we
	# must allocate 8 slots to keep the stack aligned but the 7 slots used must be those with the lowest address
	# for this to work correctly.
	#
	# Normally we would subtract 1 to get the count of parms to be on the stack then round that to a multiple of
	# 2 since 2 parms would be 16 bytes. The rounding would add 1 and shift but to avoid -1 then +1, just do the
	# rounding "AND" instruction on the value we have. Use REG64_ARG/ARG5 as temporary work registers.
	#
	cltq						# Convert REG32_ACCUM to REG64_ACCUM
	movq	REG64_ACCUM, REG64_ARG5			# Copy of parmcnt
	andq	$-2, REG64_ARG5				# Round to even value
	shlq	$3, REG64_ARG5				# Mult by 8 via shifting gives us 16 byte aligned value
	subq	REG64_ARG5, REG_SP			# Allocate aligned parm area on stack
	CHKSTKALIGN					# Verify stack alignment
	movq	REG_SP, REG64_ARG4			# Save bottom of allocation to REG64_ARG4
	subq	$2, REG64_ACCUM				# Remove one parm to be passed in parmreg and one for 0 origin
	leaq	(REG64_ARG4, REG64_ACCUM, 8), REG64_ARG4 # Address for last actuallist parm to be stored
	cmpq	$0, REG64_ACCUM				# Only 1 arg left?
	je	arg1
	leaq	(%rbp, REG64_ACCUM, 8), REG64_SCRATCH1	# Address of last passed-in parameter
again:
	movq	0(REG64_SCRATCH1), REG64_ARG5		# Move parm to temp register
	movq	REG64_ARG5, 0(REG64_ARG4)		# Move parm to home in stack location
	subq	$8, REG64_SCRATCH1			# Move pointers to previous argument
	subq	$8, REG64_ARG4
	subq	$1, REG64_ACCUM				# Count down the parm
	cmpq	$0, REG64_ACCUM				# See if down to 1 parm and if so, fall thru to handle it
	jg	again
arg1:
	movq	arg1_off(%rbp), REG64_SCRATCH1		# Copy parm to register
	movq	REG64_SCRATCH1, 0(REG64_ARG4)		# Copy parm to stack resident location
arg0:
	movq	arg0_off(%rbp), REG64_ARG5  		# Only one argument which can be fitted into REG5
no_arg:
	movl	act_cnt(%rbp), REG32_ARG4		# Actual Arg cnt
	movl	mask_arg(%rbp), REG32_ARG3		# Mask
	movq	ret_val(%rbp), REG64_ARG2		# ret_value
	movl	dollar_truth(REG_IP), REG32_ARG1	# $TEST
	andl	$1, REG32_ARG1
	movl	act_cnt(%rbp), REG32_ARG0
	addl	$4, REG32_ARG0				# Totalcount = Act count +4
	movb	$0, REG8_ACCUM				# variable length argument
	call	push_parm				# push_parm(total, $T, ret_value, mask, argc [,arg1, arg2, ...])
done:
	#
	# Ready to return - need to pick up allocated temp area and new frame pointer value (%rbp)
	#
	movq    frame_pointer(REG_IP), REG64_SCRATCH1
	movq	msf_temps_ptr_off(REG64_SCRATCH1), REG_FRAME_TMP_PTR
	movq	%rbp, REG_SP				# Unwind C stack back to caller
	movq    REG64_SCRATCH1, REG_FRAME_POINTER	# Resets %rbp aka REG_FRAME_POINTER with frame just created
	ret

error:
	movl	ERR_GTMCHECK(REG_IP), REG32_ARG1
	movl	$1, REG32_ARG0
	movb	$0, REG8_ACCUM             		# Variable length argument
	call	rts_error
	jmp	done					# Shouldn't return but in case..
