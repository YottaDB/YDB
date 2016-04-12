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
	.include "gtm_threadgbl_deftypes_asm.si"
	.include "debug.si"
#
# op_extexfun calls an external GT.M MUMPS routine with arguments and provides for
# a return value in most instances. If the routine has not yet been linked into the
# current image, op_extexfun will first link it by invoking the auto-ZLINK function.
# Before driving the function, we check if *any* routines have been linked in as
# autorelink-enabled or if any directories are autorelink-enabled and if so, drive
# a check to see if a newer version exists that could be linked in.
#
# Parameters:
#
#  - arg0 (%rdi) - Index into linkage table of caller containing the routine header
#	           address of the routine to call (rtnidx).
#  - arg1 (%rsi) - Index into linkage table of caller containing the address of an
#		   offset into routine to which to transfer control associated with
#	           a given label. This value is typically the address of the lnr_adr
#		   field in a label entry (lblidx).
#  - arg2 (%rdx) - Address of where return value is placed or NULL if none (ret_value).
#  - arg3 (%rcx) - Bit mask with 1 bit per argument (ordered low to high). When bit is
#		   set, argument is pass-by-value, else pass-by-reference (mask).
#  - arg4 (%r8)  - Count of M routine parameters supplied (actualcnt).
#  - arg5 (%r9)  - List of addresses of mval parameters (actual1).
#
# Note if lblidx (arg1) is negative, this means the linkage table to use is not from the
# caller but is contained in TREF(lnk_proxy) used by indirects and other dynamic
# code (like callins).
#
# Note the return address is also supplied (on the stack) but we remove that immediately
# since we do not return directly to it but to the the called rtn when the return address
# is loaded out of the top M stackframe by getframe.
#
# Note we use %r12 as a flag that we don't do more than one of auto_zlink() OR auto_relink_check()
# call. This keeps us out of any possible loop condition as only one or the other should
# ever be necessary. Register %r12 is also known as REG_LITERAL_BASE and is saved by the putframe
# macro so we need not save it separately.
#
	.data
	.extern	ERR_FMLLSTMISSING
	.extern	ERR_GTMCHECK
	.extern	dollar_truth
	.extern	frame_pointer
	.extern gtm_threadgbl

	.text
	.extern	auto_zlink
	.extern auto_relink_check
	.extern	new_stack_frame
	.extern	push_parm
	.extern	rts_error
	.extern laberror

arg0_off	= -40
act_cnt		= -32
mask_arg	= -28
ret_val		= -24
lblidx		= -16
rtnidx		= -8
SAVE_SIZE	= 48						# This size 16 byte aligns the stack

ENTRY	op_extexfun
	putframe						# Save registers into current M stack frame
	addq    $8, REG_SP            				# Burn return PC (also 16 byte aligns the stack)
	CHKSTKALIGN						# Verify stack alignment
	movq	$0, %r12					# We haven't done auto_zlink/auto_relink_check
	movq	REG_SP, %rbp					# Copy stack pointer to %rbp
	subq	$SAVE_SIZE, REG_SP				# Allocate save area (16 byte aligned)
	#
	# Note from here down, do *not* use REG_FRAME_POINTER which was overwritten above. REG_FRAME_POINTER  is an alias for
	# register %rbp which contains a copy of %rsp before %rsp was decremented by the save area size so %rbp contains a
	# pointer just past the save area we've allocated which is why all references are using negative offsets.
	#
	movslq	REG32_ARG1, REG64_ARG1				# Sign extend arg1 so can check for negative arg
        movq    REG64_ARG0, rtnidx(%rbp)			# Save argument registers
        movq    REG64_ARG1, lblidx(%rbp)
        movq    REG64_ARG2, ret_val(%rbp)
        movl    REG32_ARG3, mask_arg(%rbp)
	movl	REG32_ARG4, act_cnt(%rbp)
	movq    REG64_ARG5, arg0_off(%rbp)
	#
	# First up, check the label index to see if tiz negative. If so, we must use lnk_proxy as a base address
	# and pseudo linkagetable. Else use the caller's linkage table.
	#
	cmpq	$0, REG64_ARG1					# Use current frame linkage table or lnk_proxy?
	jge	loadandgo
	#
	# We have a negative index. Use lnk_proxy as a proxy linkage table.
	#
	movq	gtm_threadgbl(REG_IP), REG64_RET1		# REG64_RET1 contains threadgbl base
	leaq	ggo_lnk_proxy(REG64_RET1), REG64_ACCUM		# -> &lnk_proxy.rtnhdr_adr
	cmpq	$0, REG64_ARG0					# Using proxy table, rtnhdr index must be 0
	jne	gtmcheck
	movq	(REG64_ACCUM), REG64_ARG0			# -> rtnhdr
	cmpq	$0, REG64_ARG0					# See if defined yet
	je	gtmcheck					# If rhdaddr == 0, not yet linked into image which
								# .. should never happen for indirects
	cmpq	$-1, REG64_ARG1					# Using proxy table, label index must be -1
	jne	gtmcheck
	leaq	8(REG64_ACCUM), REG64_ARG1			# -> label table code offset ptr
	cmpq    $0, 0(REG64_ARG1)
        je      gtmcheck					# If labaddr == 0 && rhdaddr != 0, label does not exist
								# .. which also should never happen for indirects
	cmpl	$0, 16(REG64_ACCUM)				# See if a parameter list was supplied
	je	fmllstmissing					# If not, raise error
	jmp	justgo						# Bypass autorelink check for indirects (done by caller)
	#
	# We have a non-negative index. Use args as indexes into caller's linkage table. Note we cannot overwrite
	# REG_FRAME_POINTER as it is being used as %rbp in this routine (copy of %rsp before allocating save area above).
	#
loadandgo:
	movq	frame_pointer(%rip), REG_RET0			# -> frame_pointer
	movq	msf_rvector_off(REG_RET0), REG64_RET0		# -> frame_pointer->rvector (rtnhdr)
	movq	mrt_lnk_ptr(REG64_RET0), REG64_RET0		# -> frame_pointer->rvector->linkage_adr
	shlq	$3, REG64_ARG0					# arg * 8 = offset for rtnhdr ptr
	cmpq	$0, (REG64_RET0, REG64_ARG0)			# See if defined
	je	autozlink					# No - try auto-zlink
	movq	(REG64_RET0, REG64_ARG0), REG64_ARG0		# -> rtnhdr
	#
	# Have rtnhdr to call now. If rtnhdr->zhist, we should do an autorelink check on this routine to see if it needs
	# to be relinked. Only do this if %r12 is 0 meaning we haven't already done an autorelink check or if we just
	# loaded the routine via auto_zlink.
	#
	cmpq	$0, %r12					# Already checked/resolved?
	jne	getlabeloff					# Yes, bypass this check and resolve the label offset
	cmpq	$0, mrt_zhist(REG64_ARG0)			# See if we need to do an autorelink check
	jne	autorelink_check				# Need autorelink check
getlabeloff:
	shlq	$3, REG64_ARG1					# arg * 8 = offset for label offset pointer
	cmpq	$0, (REG64_RET0, REG64_ARG1)			# See if defined
	je	label_missing
	movq	(REG_RET0, REG64_ARG1), REG64_ARG1		# -> label table code offset
	cmpl	$0, 8(REG64_ARG1)				# If has_parms == 0, then issue an error
	je	fmllstmissing
	#
	# Create stack frame and invoke routine
	#
justgo:
	movq	0(REG64_ARG1), REG64_ACCUM			# &(code_offset) for this label (usually & of lntabent)
	cmpq	$0, REG64_ACCUM
	je	label_missing
	movslq	0(REG64_ACCUM), REG64_ARG2			# Code offset for this label
	addq	mrt_ptext_adr(REG64_ARG0), REG64_ARG2		# Transfer address: codebase reg + offset to label
	movq	mrt_lnk_ptr(REG64_ARG0), REG64_ARG1		# Linkage table address (context pointer)
	call	new_stack_frame
	#
	# Move parameters into place
	#
	movl	act_cnt(%rbp), REG32_ACCUM			# Number of actuallist parms
	cmpl	$0, REG32_ACCUM
	je	no_arg						# There are no actuallist parms
	cmpl	$1, REG32_ACCUM					# See if just one arg left
        je     	one_arg						# Just one - load it from the saved parm regs
	#
	# We need one or more actuallist parms to reside on the stack as we have overflowed the 6 parameter registers. We
	# need to allocate a 16 byte aligned chunk of stack memory to house those parms. If some of that block is actually
	# padding, the parms need to live in the lower address slots to work correctly with C varargs. So for example, if
	# we need 7 slots, we have to allocate 8 slots and use the lowest addressed 7 slots for vargs to work correctly.
	#
	# Normally we would subtract 1 to get the count of parms to be on the stack then round that to a multiple of
	# 2 since 2 parms would be 16 bytes. The rounding would add 1 and shift but to avoid -1 then +1, just do the
	# rounding "AND" on the value we have. Use REG64_ARG/ARG5 as temporary work registers.
	#
        cltq							# Convert REG32_ACCUM to REG64_ACCUM
	movq	REG64_ACCUM, REG64_ARG5				# Copy of parmcnt
	andq	$-2, REG64_ARG5					# Round to even value
	shlq	$3, REG64_ARG5					# Mult by 8 via shifting gives us 16 byte aligned value
	subq	REG64_ARG5, REG_SP				# Allocate aligned parm area on stack
	CHKSTKALIGN						# Verify stack alignment
	movq	REG_SP, REG64_ARG4				# Save bottom of allocation to REG64_ARG4
	subq	$2, REG64_ACCUM					# Remove one parm to be passed in parmreg and one for 0 origin
	leaq	(REG64_ARG4, REG64_ACCUM, 8), REG64_ARG4	# Address for last actuallist parm to be stored
	leaq	(%rbp, REG64_ACCUM, 8), REG64_SCRATCH1		# Address of last passed-in parameter
again:
	movq	0(REG64_SCRATCH1), REG64_ARG5			# Move parm to temp register
	movq	REG64_ARG5, 0(REG64_ARG4)			# Move parm to home in stack location
	subq	$8, REG64_SCRATCH1				# Move pointers to previous argument
	subq	$8, REG64_ARG4
	subq	$1, REG64_ACCUM					# Count down the parm
	cmpq	$0, REG64_ACCUM					# Branch unless all parms are done (0 > REG64_ACCUM)
	jnl	again
one_arg:
	movq    arg0_off(%rbp), REG64_ARG5
no_arg:
	movl	act_cnt(%rbp), REG32_ARG4
	movl    mask_arg(%rbp), REG32_ARG3
        movq    ret_val(%rbp), REG64_ARG2
	movl   	dollar_truth(REG_IP), REG32_ARG1
	andl   	$1, REG32_ARG1
	movl   	act_cnt(%rbp), REG32_ARG0
	addl   	$4, REG32_ARG0					# Includes: $TEST, ret_value, mask, act_cnt
	movb    $0, REG8_ACCUM					# Variable length argument
	call	push_parm					# push_parm (total, $T, ret_value, mask, argc [,arg1, arg2, ...]);
retlab:
	movq	%rbp, REG_SP					# Unwind C stack back to caller
	getframe						# Load regs from top M frame (pushes return reg)
	ret

#
# Drive auto_zlink to fetch module
#
autozlink:
	cmpq	$0, %r12					# Already did autorelink or autorelink check?
	jne	gtmcheck
	movq	rtnidx(%rbp), REG64_ARG0			# Get index arg back
	call	auto_zlink
	movq	rtnidx(%rbp), REG64_ARG0			# Restore both args after call
	movq	lblidx(%rbp), REG64_ARG1
	movq	$1, %r12
	jmp	loadandgo

#
# Drive auto_relink_check to see if a newer routine should be loaded
#
autorelink_check:
	cmpq	$0, %r12					# Already did autorelink or autorelink check?
	jne	gtmcheck
	movq	rtnidx(%rbp), REG64_ARG0			# Restore both args as parms for call
	movq	lblidx(%rbp), REG64_ARG1
	call	auto_relink_check				# REG64_ARG0 still populated by rtnhdr
	movq	rtnidx(%rbp), REG64_ARG0			# Restore both args after call
	movq	lblidx(%rbp), REG64_ARG1
	movq	$2, %r12
	jmp	loadandgo

#
# Raise GTMCHECK (pseudo-GTMASSERT since args are more difficult in assembler) when something really screwedup
# occurs
#
gtmcheck:
	movl	ERR_GTMCHECK(REG_IP), REG32_ARG1
	movl	$1, REG32_ARG0
	movb    $0, REG8_ACCUM					# Variable length argument
	call	rts_error
	jmp	retlab

#
# Make call so we can raise the appropriate LABELMISSING error for the not-found label.
#
label_missing:
	movq	lblidx(%rbp), REG64_ARG0			# Index to linkage table and to linkage name table
	call	laberror
	jmp	retlab

#
# Raise missing formal list error
#
fmllstmissing:
	movl    ERR_FMLLSTMISSING(REG_IP), REG32_ARG1
        movl    $1, REG32_ARG0
	movb	$0, REG8_ACCUM					# Variable length argument
	call	rts_error
	jmp	retlab
