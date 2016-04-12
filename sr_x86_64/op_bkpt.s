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
	.extern	_frame_pointer
	.extern	_zstep_level

	.text
	.extern	_gtm_fetch
	.extern	_op_retarg
	.extern	_op_zbreak
	.extern	_op_zst_break
	.extern	_op_zst_over
	.extern	_op_zstepret
	.extern	_opp_ret

ENTRY	_opp_zstepret
	subq	$8, %rsp				# Align stack to 16 bytes
	CHKSTKALIGN					# Verify stack alignment
	movq	_frame_pointer(%rip), %rax
	movw	msf_typ_off(%rax), %r11w
	testw	$1, %r11w
	je	l1
	movq	_zstep_level(%rip), %rdx
	cmpq	%rax, %rdx
	jg	l1
	call	_op_zstepret
l1:
	addq	$8, %rsp				# Remove stack alignment bump
	jmp	_opp_ret

ENTRY	_opp_zstepretarg
	subq	$24, %rsp				# Align stack to 16 bytes plus 2 long int save areas
	CHKSTKALIGN					# Verify stack alignment
	movq	%rax, save0(%rsp)		# Save input regs
	movq	%r10, save1(%rsp)
	movq	_frame_pointer(%rip), %rax
	movw	msf_typ_off(%rax), %dx
	testw	$1, %dx
	je	l2
	movq	_zstep_level(%rip), %rdx
	cmpq	%rax, %rdx
	jg	l2
	call	_op_zstepret
l2:
	movq	save1(%rsp), %r10		# Restore input regs
	movq	save0(%rsp), %rax
	addq	$24, %rsp				# Remove our stack bump
	jmp	_op_retarg

ENTRY	_op_zbfetch
	movq	_frame_pointer(%rip), %rax
	popq	msf_mpc_off(%rax)		# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	movb    $0, %al             		# Variable length argumentt
	call	_gtm_fetch
	movq	_frame_pointer(%rip), %rdi
	call	_op_zbreak
	getframe					# Pushes return addr on stack
	ret

ENTRY	_op_zbstart
	movq	_frame_pointer(%rip), %rdi
	popq	msf_mpc_off(%rdi)			# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	call	_op_zbreak
	getframe					# Pushes return addr on stack
	ret

ENTRY	_op_zstepfetch
	movq	_frame_pointer(%rip), %rax
	popq	msf_mpc_off(%rax)		# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	movb    $0, %al             		# Variable length argument
	call	_gtm_fetch
	call	_op_zst_break
	getframe					# Pushes return addr on stack
	ret

ENTRY	_op_zstepstart
	movq	_frame_pointer(%rip), %rdx
	popq	msf_mpc_off(%rdx)			# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	call	_op_zst_break
	getframe					# Pushes return addr on stack
	ret

ENTRY	_op_zstzbfetch
	movq	_frame_pointer(%rip), %rax
	popq	msf_mpc_off(%rax)		# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	movb    $0, %al             		# Variable length argument
	call	_gtm_fetch
	movq	_frame_pointer(%rip), %rdi
	call	_op_zbreak
	call	_op_zst_break
	getframe
	ret

ENTRY	_op_zstzbstart
	movq	_frame_pointer(%rip), %rdi
	popq	msf_mpc_off(%rdi)			# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	call	_op_zbreak
	call	_op_zst_break
	getframe					# Pushes return addr on stack
	ret

ENTRY	_op_zstzb_fet_over
	movq	_frame_pointer(%rip), %rax
	popq	msf_mpc_off(%rax)		# Save return address and remove from stack (now 16 byte aligned)
	movb    $0, %al             		# Variable length argument
	CHKSTKALIGN					# Verify stack alignment
	call	_gtm_fetch
	movq	_frame_pointer(%rip), %rdi
	call	_op_zbreak
	movq	_zstep_level(%rip), %rdx
	movq    _frame_pointer(%rip), %r11
	cmpq	%r11, %rdx
	jae	l3
	cmpl	$0, %eax
	jne	l5
	jmp	l4
l3:
	call	_op_zst_break
l4:
	getframe					# Pushes return addr on stack
	ret
l5:
	call	_op_zst_over
	movq	_frame_pointer(%rip), %rdx
	pushq	msf_mpc_off(%rip)			# Restore return address
	ret

ENTRY	_op_zstzb_st_over
	movq	_frame_pointer(%rip), %rdi
	popq	msf_mpc_off(%rdi)			# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	call	_op_zbreak
	movq	_zstep_level(%rip), %rdi
	movq	_frame_pointer(%rip), %r11
	cmpq	%r11, %rdi
	jae	l6
	cmpl	$0, %eax
	jne	l8
	jmp	l7
l6:
	call	_op_zst_break
l7:
	getframe					# Pushes return addr on stack
	ret
l8:
	call	_op_zst_over
	movq	_frame_pointer(%rip), %rdi
	pushq	msf_mpc_off(%rdi)			# Restore return address
	ret

ENTRY	_op_zst_fet_over
	movq	_frame_pointer(%rip), %rax
	popq	msf_mpc_off(%rax)		# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	movb    $0, %al				# Variable length argument
	call	_gtm_fetch
	movq	_zstep_level(%rip), %rax
	movq	_frame_pointer(%rip), %r11
	cmpq	%r11, %rax
	jg	l9
	call	_op_zst_break
	getframe					# Pushes return addr on stack
	ret
l9:
	call	_op_zst_over
	movq	_frame_pointer(%rip), %rax
	pushq	msf_mpc_off(%rax)		# Restore return address
	ret

ENTRY	_op_zst_st_over
	movq	_frame_pointer(%rip), %rax
	popq	msf_mpc_off(%rax)		# Save return address and remove from stack (now 16 byte aligned)
	CHKSTKALIGN					# Verify stack alignment
	movq	_zstep_level(%rip), %rdx
	cmpq	%rax, %rdx
	jg	l10
	call	_op_zst_break
	getframe					# Pushes return addr on stack
	ret
l10:
	call	_op_zst_over
	movq	_frame_pointer(%rip), %rdx
	pushq	msf_mpc_off(%rdx)			# Restore return address
	ret

ENTRY	_opp_zst_over_ret
	subq	$8, %rsp				# Align stack to 16 bytes
	CHKSTKALIGN					# Verify stack alignment
	movq	_frame_pointer(%rip), %rax
	movw	msf_typ_off(%rax), %dx
	testw	$1, %dx
	je	l11
	movq	_zstep_level(%rip), %rdx
	movq	msf_old_frame_off(%rax), %rax
	cmpq	%rax, %rdx
	jg	l11
	call	_op_zstepret
l11:
	addq	$8, %rsp				# Remove stack alignment bump
	jmp	_opp_ret

ENTRY	_opp_zst_over_retarg
	subq	$24, %rsp				# Align stack to 16 bytes plus 2 long int save areas
	CHKSTKALIGN					# Verify stack alignment
	movq	%rax, save0(%rsp)		# Save input regs
	movq	%r10, save1(%rsp)
	movq	_frame_pointer(%rip), %rax
	movw	msf_typ_off(%rax), %dx
	testw	$1, %dx
	je	l12
	movq	_zstep_level(%rip), %rdx
	movq	msf_old_frame_off(%rax), %rax
	cmpq	%rax, %rdx
	jg	l12
	call	_op_zstepret
l12:
	movq	save1(%rsp), %r10		# Restore input regs
	movq	save0(%rsp), %rax
	addq	$24, %rsp				# Remove our stack bump
	jmp	_op_retarg
