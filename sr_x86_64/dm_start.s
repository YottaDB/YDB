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

	.DATA
	.extern	dollar_truth
	.extern	xfer_table
	.extern	frame_pointer
	.extern	msp
	.extern	mumps_status
	.extern	restart

	.text
	.extern	mdb_condition_handler
	.extern	op_unwind
	.extern __sigsetjmp			# setjmp() is really __sigsetjmp(env,0)

ENTRY	dm_start
	pushq	%rbp				# Preserve caller's %rbp register (aka REG_STACK_FRAME) which 16 byte aligns stack
	subq	$SUPER_STACK_SIZE, REG_SP	# Create super-stack-frame with room for many args
	save_callee_saved
	CHKSTKALIGN				# Verify stack alignment
	movq    REG_FRAME_POINTER, REG_FRAME_POINTER_SAVE_OFF(REG_SP)  # Save the %rbp value, as that will be trashed in the runtime
	movq	REG_XFER_TABLE, REG_XFER_TABLE_SAVE_OFF(REG_SP)
	movl    $1, mumps_status(REG_IP)
	leaq	xfer_table(REG_IP), REG_XFER_TABLE
	movl	$1, dollar_truth(REG_IP)
	ESTABLISH l30
	movq    restart(REG_IP), REG64_SCRATCH1
	call    *REG64_SCRATCH1
return:
	movl	mumps_status(REG_IP), REG32_ACCUM
	movq	REG_XFER_TABLE_SAVE_OFF(REG_SP), REG_XFER_TABLE
	movq	REG_FRAME_POINTER_SAVE_OFF(REG_SP), REG_FRAME_POINTER  # Restore the %rbp value, as it will be trashed in runtime
	restore_callee_saved
	addq	$SUPER_STACK_SIZE, REG_SP	# Unwind super stack
	popq	%rbp				# Restore caller's %rbp register
	ret

ENTRY	gtm_ret_code
	CHKSTKALIGN				# Verify stack alignment
	REVERT
	call	op_unwind
	movq	msp(REG_IP), REG64_ACCUM
	movq	(REG64_ACCUM), REG64_ACCUM
	movq	REG64_ACCUM, frame_pointer(REG_IP)
	addq	$8, msp(REG_IP)
	jmp	return

ENTRY	gtm_levl_ret_code
	CHKSTKALIGN				# Verify stack alignment
	REVERT
	jmp	return
