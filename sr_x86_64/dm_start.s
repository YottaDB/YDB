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
	.include "error.si"
	.include "debug.si"

	.data
	.extern	_dollar_truth
	.extern	_xfer_table
	.extern	_frame_pointer
	.extern	_msp
	.extern	_mumps_status
	.extern	_restart

	.text
	.extern	_mdb_condition_handler
	.extern	_op_unwind
	.extern __sigsetjmp			# setjmp() is really __sigsetjmp(env,0)

ENTRY	_dm_start
	pushq	%rbp				# Preserve caller's %rbp register (aka REG_STACK_FRAME) which 16 byte aligns stack
	subq	$SUPER_STACK_SIZE, %rsp	# Create super-stack-frame with room for many args
	save_callee_saved
	CHKSTKALIGN				# Verify stack alignment
	movq    %rbp, REG_FRAME_POINTER_SAVE_OFF(%rsp)  # Save the %rbp value, as that will be trashed in the runtime
	movq	%rbx, REG_XFER_TABLE_SAVE_OFF(%rsp)
	movl    $1, _mumps_status(%rip)
	leaq	_xfer_table(%rip), %rbx
	movl	$1, _dollar_truth(%rip)
	ESTABLISH l30
	movq    _restart(%rip), %r11
	call    *%r11
return:
	movl	_mumps_status(%rip), %eax
	movq	REG_XFER_TABLE_SAVE_OFF(%rsp), %rbx
	movq	REG_FRAME_POINTER_SAVE_OFF(%rsp), %rbp  # Restore the %rbp value, as it will be trashed in runtime
	restore_callee_saved
	addq	$SUPER_STACK_SIZE, %rsp	# Unwind super stack
	popq	%rbp				# Restore caller's %rbp register
	ret

ENTRY	_gtm_ret_code
	CHKSTKALIGN				# Verify stack alignment
	REVERT
	call	_op_unwind
	movq	_msp(%rip), %rax
	movq	(%rax), %rax
	movq	%rax, _frame_pointer(%rip)
	addq	$8, _msp(%rip)
	jmp	return

ENTRY	_gtm_levl_ret_code
	CHKSTKALIGN				# Verify stack alignment
	REVERT
	jmp	return
