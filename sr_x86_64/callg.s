#################################################################
#								#
# Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	#
# All rights reserved.						#
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
# Routine to make a correct varargs type call from a parm list in an array. For this type of call, the parameters
# that are not passed in registers (the first 6 params), are the lowest address of the caller's stack frame.
# The only thing between the caller's frame and the current frame is the return address pushed on the stack by the
# actual call (see below).
#
# Parameters:
#   1. Address of routine to be called
#   2. Address of gparam_list parm block (first entry holds count, remainder addresses of parameters)
#
# Return value is whatever is returned from the callee.
#
# ASCII art of C stack of varargs arrangement for theoretical 8 argument call which means a total of 9 actual arguments
# because the first argument is a count:
#
#       [high address]
#            ...
#    *-------------------------------*
#    |                               |
#    |  caller's (callg) stack frame |
#    |                               |
#    |  arg8 (8th arg)               |
#    |  arg7 (7th arg)               |
#    |  arg6 (6th arg)               |
#    |-------------------------------|
#    |  caller's return address      |
#    *-------------------------------*
#    |  callee's stack frame         | [arg0 - arg5 loaded in registers - arg0 has count]
#    *-------------------------------*
#            ...
#       [low address]
#
	.data
	.extern ERR_GTMCHECK

	.text
	.extern	rts_error

ARGS_IN_REGS 		= 6				# Count of arguments passed in registers for x86-64
CALLEE_ARGS_IN_REGS	= (ARGS_IN_REGS - 1)		# Since pass count parm in arg0, caller reg parms reduced by 1
MAX_ARGS		= 32				# Maximum number of M arguments
OVERHEAD_ARGS		= 4				# Overhead arguments used (max of 4 by push_parm())
MAX_TOTAL_ARGS		= (MAX_ARGS + OVERHEAD_ARGS)	# Total args we are expected to deal with

ENTRY	callg
	pushq	%rbp					# Save previous C frame pointer or M stack pointer
							# .. also aligns stack to 16 bytes (return addr on stack)
							# .. (part of standard linkage)
	CHKSTKALIGN					# Verify we are 16 byte aligned
	movq	%rsp, %rbp				# Save previous frame address (standard linkage)
	movq	REG64_ARG0, REG64_SCRATCH0		# Save routine to be called in a scratch register
	movq	REG64_ARG1, REG64_ACCUM			# Input gparam_list base address
	movq	0(REG64_ACCUM), REG64_ARG0		# Count of parameters (passed to callee)
	#
	# Verify not too many arguments passed in
	#
	cmpq	$MAX_TOTAL_ARGS, REG64_ARG0		# Too many arguments specified?
	ja	ERRTOOMANYARGS
	#
	# Test our count of parms over 6 (which includes 1 parm for count) to verify stack alignment after parm pushes.
	# Note %rcx and %rdx are argument registers we are using as temps until args are actually loaded into them.
	#
	cmpq	$CALLEE_ARGS_IN_REGS, REG64_ARG0
	jle	nostackparms				# No stack parms this time so no stack alignment issue
	movq	REG64_ARG0, %rcx			# Copy count for manipulation
	#
	# At this point, %rcx holds the number of parms we will push on the stack. These are 8 byte parms so if the
	# count is even, the stack will be aligned after the pushes. If odd, the stack won't be and we need to make
	# an adjustment so it is.
	subq	$CALLEE_ARGS_IN_REGS, %rcx		# Compute stack resident args in %rcx
	testq	$1, %rcx				# If even, tiz 16 byte aligned - if odd, isn't and needs a bump
	jz	stackisaligned				# Branch around stack realignment if %rcx is even (aligned)
	subq	$8, %rsp				# It is not aligned so align it by allocating 8 more bytes
stackisaligned:
nostackparms:
	#
	# Now compute branch offset to load parms
	#
	leaq	callgbrtbl(%rip), %rdx			# Address of branch table
	movslq	(%rdx, REG64_ARG0, 4), %rcx		# Load offset from branch table
	addq	%rcx, %rdx				# Add offset to branch table base to get jump point
	jmp	*%rdx					# Branch to branch table offset location
	#
	# This branch offset table is indexed by the number of input arguments so we can load the correct number
	# of arguments. Note this branch offset table is actually in a separate R/O section. The double alignment
	# using align and p2align assembler directives is a technique seen in the gcc assembler output.
	#
	.section	.rodata
	.align	4
	.align	4
callgbrtbl:
	.long	callg00args - callgbrtbl
	.long	callg01args - callgbrtbl
	.long	callg02args - callgbrtbl
	.long	callg03args - callgbrtbl
	.long	callg04args - callgbrtbl
	.long	callg05args - callgbrtbl
	.long	callg06args - callgbrtbl
	.long	callg07args - callgbrtbl
	.long	callg08args - callgbrtbl
	.long	callg09args - callgbrtbl
	.long	callg10args - callgbrtbl
	.long	callg11args - callgbrtbl
	.long	callg12args - callgbrtbl
	.long	callg13args - callgbrtbl
	.long	callg14args - callgbrtbl
	.long	callg15args - callgbrtbl
	.long	callg16args - callgbrtbl
	.long	callg17args - callgbrtbl
	.long	callg18args - callgbrtbl
	.long	callg19args - callgbrtbl
	.long	callg20args - callgbrtbl
	.long	callg21args - callgbrtbl
	.long	callg22args - callgbrtbl
	.long	callg23args - callgbrtbl
	.long	callg24args - callgbrtbl
	.long	callg25args - callgbrtbl
	.long	callg26args - callgbrtbl
	.long	callg27args - callgbrtbl
	.long	callg28args - callgbrtbl
	.long	callg29args - callgbrtbl
	.long	callg30args - callgbrtbl
	.long	callg31args - callgbrtbl
	.long	callg32args - callgbrtbl
	.long	callg33args - callgbrtbl
	.long	callg34args - callgbrtbl
	.long	callg35args - callgbrtbl
	.long	callg36args - callgbrtbl
	#
	# Back to the text section now. Again, the double alignment is as seen in gcc assembler listings where
	# a computed branch was used. Define a labeled argument load instruction for each label in the branch
	# offset table above.
	#
	# Note arg0 (REG64_ARG0 aka %rdi) has already been setup with the count of remaining arguments.
	#
	.text
	.p2align 4,,10
	.p2align 3
	#
callg36args:	pushq	288(REG64_ACCUM)		# Push argument 36
callg35args:	pushq	280(REG64_ACCUM)		# Push argument 35
callg34args:	pushq	272(REG64_ACCUM)		# Push argument 34
callg33args:	pushq	264(REG64_ACCUM)		# Push argument 33
callg32args:	pushq	256(REG64_ACCUM)		# Push argument 32
callg31args:	pushq	248(REG64_ACCUM)		# Push argument 31
callg30args:	pushq	240(REG64_ACCUM)		# Push argument 30
callg29args:	pushq	232(REG64_ACCUM)		# Push argument 29
callg28args:	pushq	224(REG64_ACCUM)		# Push argument 28
callg27args:	pushq	216(REG64_ACCUM)		# Push argument 27
callg26args:	pushq	208(REG64_ACCUM)		# Push argument 26
callg25args:	pushq	200(REG64_ACCUM)		# Push argument 25
callg24args:	pushq	192(REG64_ACCUM)		# Push argument 24
callg23args:	pushq	184(REG64_ACCUM)		# Push argument 23
callg22args:	pushq	176(REG64_ACCUM)		# Push argument 22
callg21args:	pushq	168(REG64_ACCUM)		# Push argument 21
callg20args:	pushq	160(REG64_ACCUM)		# Push argument 20
callg19args:	pushq	152(REG64_ACCUM)		# Push argument 19
callg18args:	pushq	144(REG64_ACCUM)		# Push argument 18
callg17args:	pushq	136(REG64_ACCUM)		# Push argument 17
callg16args:	pushq	128(REG64_ACCUM)		# Push argument 16
callg15args:	pushq	120(REG64_ACCUM)		# Push argument 15
callg14args:	pushq	112(REG64_ACCUM)		# Push argument 14
callg13args:	pushq	104(REG64_ACCUM)		# Push argument 13
callg12args:	pushq	96(REG64_ACCUM)			# Push argument 12
callg11args:	pushq	88(REG64_ACCUM)			# Push argument 11
callg10args:	pushq	80(REG64_ACCUM)			# Push argument 10
callg09args:	pushq	72(REG64_ACCUM)			# Push argument 9
callg08args:	pushq	64(REG64_ACCUM)			# Push argument 8
callg07args:	pushq	56(REG64_ACCUM)			# Push argument 7
callg06args:	pushq	48(REG64_ACCUM)			# Push argument 6
callg05args:	movq	40(REG64_ACCUM), REG64_ARG5	# Move argument 5 to argument register
callg04args:	movq	32(REG64_ACCUM), REG64_ARG4	# Move argument 4 to argument register
callg03args:	movq	24(REG64_ACCUM), REG64_ARG3	# Move argument 3 to argument register
callg02args:	movq	16(REG64_ACCUM), REG64_ARG2	# Move argument 2 to argument register
callg01args:	movq	8(REG64_ACCUM), REG64_ARG1	# Move argument 1 to argument register
	#
	# Arguments now loaded - drive the call
	#
callg_drivecall:					# Primarily just so gdb doesn't report further addresses as
							# .. based off of callg01args above
	xorl	%eax, %eax				# No floating point registers in parms
	call	*REG64_SCRATCH0				# Drive the chosen routine with its arguments
retlab:							# Unwind and return to caller
	leave						# Restore %rsp, then pop saved %rbp thus unwinding parms from stack
	ret						# Return to caller
	.p2align 4,,10
	.p2align 3
	#
	# When there are no args, the call is a bit simpler so special case it here. Since the stack is (or will be)
	# as it was when we came in (after the leave instruction) - merrily jump to the called routine taking this
	# routine out of the C stack. When the callee returns, it will return directly to our caller.
	#
callg00args:						# No arguments - just make the call
	xorl	%eax, %eax				# No floating point registers in parms
	leave	      					# Restore %rsp, then pop saved %rbp
	jmp	*REG64_SCRATCH0				# Jump to callee - No return from this transfer - callee returns
							# .. directly to our caller bypassing us entirely
	.p2align 4,,10
	.p2align 3

#
# Raise GTMCHECK if too many arguments. It's not an ideal error but we don't have the primitives we have in C that would
# allow us to identify the routine and line number it failed on with an ASSERT or other more meaningful error. This is
# not a user condition but rather an internal error that would allow too many args on this call as callg() is never
# directly called with user supplied input.
#
ERRTOOMANYARGS:
	movl	ERR_GTMCHECK(REG_IP), REG32_ARG1
	movl	$1, REG32_ARG0
	xorl	%eax, %eax				# No floating point registers in parms
	call	rts_error
	jmp	retlab

#
# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
# This marking is not an issue in Linux but is in Windows Subsystem on Linux (WSL) which does not enable executable stack.
#
.section        .note.GNU-stack,"",@progbits
