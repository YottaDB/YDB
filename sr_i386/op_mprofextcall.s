#################################################################
#								#
#	Copyright 2001, 2012 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_mprofextcall.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_mprofextcall
#	PAGE	+
	.DATA
.extern	ERR_GTMCHECK
.extern	ERR_LABELUNKNOWN
.extern	frame_pointer

	.text
.extern	auto_zlink
.extern	new_stack_frame_sp
.extern	rts_error

# PUBLIC	op_mprofextcall
ENTRY op_mprofextcall
	putframe
	addl	$4,%esp			# burn return pc
	popl	%edx			# routine hdr addr
	popl	%eax			# label addr
	cmpl	$0,%eax
	je	l2
l1:	movl	(%eax),%eax		# get the line number offset
	cmpl	$0,%eax
	je	l4
	addl	mrt_curr_ptr(%edx),%eax
	addl	%edx,%eax		# get the line number pointer
	movl	(%eax),%eax		# get the line number
	addl	mrt_curr_ptr(%edx),%eax
	addl	%edx,%eax
	pushl	%eax
	pushl	$0
	pushl	%edx
	call	new_stack_frame_sp
	addl	$12,%esp
	getframe
	ret

l2:	cmpl	$0,%edx
	jne	l4
	subl	$4,%esp
	pushl	%esp
	movl	frame_pointer,%eax
	pushl	msf_mpc_off(%eax)
	call	auto_zlink
	addl	$8,%esp
	cmpl	$0,%eax
	je	l3
	movl	%eax,%edx
	popl	%eax
	cmpl	$0,%eax
	jne	l1
l3:	addl	$4,%esp
	pushl	ERR_GTMCHECK
	pushl	$1
	call	rts_error
	pushl	$1		# in original m68020 code ??
	addl	$8,%esp
	getframe
	ret

l4:	pushl	ERR_LABELUNKNOWN
	pushl	$1
	call	rts_error
	addl	$8,%esp
	getframe
	ret
# op_mprofextcall ENDP

# END
