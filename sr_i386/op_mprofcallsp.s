#################################################################
#								#
#	Copyright 2001 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_mprofcallsp.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_mprofcallsp
#	PAGE	+
	.DATA
.extern	dollar_truth
.extern	frame_pointer

	.text
.extern	exfun_frame_push_dummy_frame
.extern	push_tval

	.sbttl	op_mprofcallspb
# PUBLIC	op_mprofcallspb
ENTRY op_mprofcallspb
	movl	frame_pointer,%edx
	movl	(%esp),%eax
	movl	%eax,msf_mpc_off(%edx)
	addl	$2,msf_mpc_off(%edx)	# store pc in MUMPS stack frame
doit:	call	exfun_frame_push_dummy_frame
	pushl	dollar_truth
	call	push_tval
	addl	$4,%esp
	movl	frame_pointer,%edx
	movl	msf_temps_ptr_off(%edx),%edi
	ret
# op_mprofcallspb ENDP

	.sbttl	op_mprofcallspw, op_mprofcallspl
# PUBLIC	op_mprofcallspw, op_mprofcallspl
ENTRY op_mprofcallspw
ENTRY op_mprofcallspl
	movl	frame_pointer,%edx
	movl	(%esp),%eax
	movl	%eax,msf_mpc_off(%edx)
	addl	$5,msf_mpc_off(%edx)	# store pc in MUMPS stack frame
	jmp	doit
# op_mprofcallspw ENDP

# END
