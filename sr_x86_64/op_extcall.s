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
# op_extcall calls an external GT.M MUMPS routine with no arguments. If the routine
# has not yet been linked into the current image, op_extcall will first link it by
# invoking the auto-ZLINK function. Before driving the function, we check if *any*
# routines have been linked in as autorelink-enabled or if any directories are
# autorelink-enabled and if so, drive a check to see if a newer version exists that
# could be linked in.
#
# Parameters:
#
#  - arg0 (%rdi) - Index into linkage table of caller containing the routine header
#	           address of the routine to call.
#  - arg1 (%rsi) - Index into linkage table of caller containing the address of an
#		   offset into routine to which to transfer control associated with
#	           a given label. This value is typically the address of the lnr_adr
#		   field in a label entry
#
# Note if arg1 is negative, this means the linkage table to use is not from the
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
	.extern	ERR_GTMCHECK
	.extern	frame_pointer
	.extern gtm_threadgbl

	.text
	.extern	auto_zlink
	.extern auto_relink_check
	.extern	new_stack_frame
	.extern	rts_error
	.extern laberror
#
# Define offsets for arguments pushed back on the stack
#
stack_arg0	= 0
stack_arg1	= 8
SAVE_SIZE	= 16

ENTRY	op_extcall
	putframe						# Save registers into current M stack frame (includes %r12)
	addq    $8, REG_SP     					# Burn the saved return pc (also aligns stack to 16 bytes)
	CHKSTKALIGN						# Verify stack alignment
	subq	$SAVE_SIZE, REG_SP				# Allocate save area on stack (16 byte aligned)
	movq	$0, %r12					# Init flag - We haven't done auto_zlink/auto_relink_check
	movslq	REG32_ARG1, REG64_ARG1				# Sign extend arg1
	movq	REG64_ARG1, stack_arg1(REG_SP)			# Save index args
	movq	REG64_ARG0, stack_arg0(REG_SP)
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
	leaq	ggo_lnk_proxy(REG64_RET1), REG64_ACCUM		# -> lnk_proxy.rtnhdr_adr
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
	jmp	justgo						# Bypass autorelink check for indirects (done by caller)
	#
	# We have a non-negative index. Use args as indexes into caller's linkage table.
	#
loadandgo:
	movq	msf_rvector_off(REG_FRAME_POINTER), REG64_RET0	# -> frame_pointer->rvector (rtnhdr)
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
	shlq	$3, REG64_ARG1					# arg * 8 = offset for label offset ptr
	cmpq	$0, (REG64_RET0, REG64_ARG1)			# See if defined
	je	label_missing
	movq	(REG_RET0, REG64_ARG1), REG64_ARG1		# -> label table code offset
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
retlab:								# If error, return to caller, else "return" to callee
	addq	$SAVE_SIZE, REG_SP				# Undo save area bump
	getframe						# Sets regs (including %r12) as they should be for new frame
	ret

#
# Drive auto_zlink to fetch module
#
autozlink:
	cmpq	$0, %r12					# Already did autorelink or autorelink check?
	jne	gtmcheck
	movq	stack_arg0(REG_SP), REG64_ARG0			# Get index arg back
	call	auto_zlink
	movq	stack_arg0(REG_SP), REG64_ARG0			# Restore both args after call
	movq	stack_arg1(REG_SP), REG64_ARG1
	movq	$1, %r12
	jmp	loadandgo

#
# Drive auto_relink_check to see if a newer routine should be loaded
#
autorelink_check:
	cmpq	$0, %r12					# Already did autorelink or autorelink check?
	jne	gtmcheck
	movq	stack_arg0(REG_SP), REG64_ARG0			# Restore both args as parms for call
	movq	stack_arg1(REG_SP), REG64_ARG1
	call	auto_relink_check				# REG64_ARG0 still populated by rtnhdr
	movq	stack_arg0(REG_SP), REG64_ARG0			# Restore both args after call
	movq	stack_arg1(REG_SP), REG64_ARG1
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
	movq	stack_arg1(REG_SP), REG64_ARG0			# Index to linkage table and to linkage name table
	call	laberror
	jmp	retlab
