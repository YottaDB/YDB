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

	.DATA
	.extern	frame_pointer

	.text
	.extern	op_tstart

save_arg5	= -16
save_arg4	= -8
FRAME_SIZE	= 16
#
# Wrapper for op_tstart that rebuffers the arguments and adds an arg to the front of the list to so op_tstart
# knows whether it was called from generated code or from C code since it handles TP restarts differently in
# those cases. This routine also saves/reloads the stackframe and related pointers because on an indirect call
# op_tstart() may shift the stack frame due to where it needs to put the TPHOST mv_stent.
#
# Parameters:
#    arg0:      (int) SERIAL flag
#    arg1:      (mval *) transaction id
#    arg2:      (int) count of local vars to be saved/restored
#    arg3-arg5: (mval *) list of (arg2) mvals (continuing on stack if needed) containing the NAMES of variables
#               to be saved and restored on a TP restart.
#
# Note no need to save %rbp in the prologue as it gets reset to the new frame when we return. Note also we subtract
# 8 from FRAME_SIZE because this routine pops the return address after putframe saves it. So instead of removing the
# stack space for the return address then adding 16 bytes for save area, just reduce by the difference.
#
ENTRY	opp_tstart
        putframe
	leaq	8(REG_SP), %rbp				# Address of start of parameter list on stack
        subq    $FRAME_SIZE-8, REG_SP 			# Burn the return pc, add 16 byte save area and 16 byte align stack
	CHKSTKALIGN					# Verify stack alignment
	movq	REG64_ARG4, save_arg4(%rbp)		# Save two parms as we need more temp registers
	movq	REG64_ARG5, save_arg5(%rbp)
        movl    REG32_ARG2, REG32_ACCUM			# Count of mval var name parms
        cmpl    $0, REG32_ACCUM
        je      no_arg
        cmpl    $1, REG32_ACCUM
        je      arg_1
        cmpl    $2, REG32_ACCUM
        je      arg_2
	#
	# We have more than 2 local variable names to save/restore so we need some aligned space on the stack for
	# parameters that don't fit in the 6 parm registers. Only the first two var name parameters can fit in
	# parm registers though a third var was initially in a parm reg but is now to be shifted to the stack since
	# this code adds a parm. All other parms s must reside on the stack starting at the lowest address. So for
	# example, if we need 7 slots, we must allocate 8 slots to keep the stack aligned but the 7 slots used must
	# be those with the lowest address for this to work correctly.
	#
	cltq						# Convert REG32_ACCUM to REG64_ACCUM
	subq	$1, REG64_ACCUM				# Two parms in regs so reduce by 1 to get "roundable" count
	movq	REG64_ACCUM, REG64_ARG5			# Copy of argument count
	andq	$-2, REG64_ARG5				# Round to even value
	shlq	$3, REG64_ARG5				# Mult by 8 via shifting gives us 16 byte aligned value
	subq	REG64_ARG5, REG_SP			# Allocate aligned parm area on stack
	CHKSTKALIGN					# Verify stack alignment
	movq	REG_SP, REG64_ARG4			# Save bottom of allocation to REG64_ARG4
	subq	$2, REG64_ACCUM				# Remove 1 (more) parm to be passed in parmreg plus 1 for 0 origin
	leaq	(REG64_ARG4, REG64_ACCUM, 8), REG64_ARG4 # Address for last actuallist parm to be stored
	cmpq	$0, REG64_ACCUM				# Only 1 arg left (zero origin)?
	je	arg_3
	leaq	(%rbp, REG64_ACCUM, 8), REG64_SCRATCH1	# Address of last passed-in parameter + 8 since 3 parms
							# .. were passed in parm registers
	subq	$8, REG64_SCRATCH1			# Correct pointer to last passed-in parm
again:
	movq	0(REG64_SCRATCH1), REG64_ARG5		# There are only 3 parms on the incoming stack
	movq	REG64_ARG5, 0(REG64_ARG4)		# Move parm to home in stack location
	subq	$8, REG64_SCRATCH1			# Move pointers to previous argument
	subq	$8, REG64_ARG4
	subq	$1, REG64_ACCUM				# Count down the parm
	cmpq	$0, REG64_ACCUM				# See if down to 1 parm and if so, fall thru to handle it
	jg	again
	#
	# When one arg is left, it is the originally passed-in arg5 now saved in save_arg5(%rbp). Since there is no
	# room for this this parm in the registers anymore, put it in its place on the stack as well.
	#
arg_3:
        movq	save_arg5(%rbp), REG64_ARG5
	movq	REG64_ARG5, 0(REG64_ARG4)		# Move parm to home in stack location
arg_2:
	movq	save_arg4(%rbp), REG64_ARG5
arg_1:
	movq	REG64_ARG3, REG64_ARG4
no_arg:
	movq	REG64_ARG2, REG64_ARG3
        movq	REG64_ARG1, REG64_ARG2
        movq	REG64_ARG0, REG64_ARG1
        movl	$0, REG32_ARG0				# arg0: NOT an implicit op_tstart() call
        movb	$0, REG8_ACCUM				# Variable length argument
        call    op_tstart
	movq	%rbp, %rsp				# Restore stack pointer unwinding save/parm area
        getframe					# Get frame pointers and push return addr
        ret
