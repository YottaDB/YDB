#################################################################
#								#
#	Copyright 2001, 2011 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_mproflinefetch.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_mproflinefetch
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	gtm_fetch
.extern	stack_leak_check
.extern pcurrpos

# PUBLIC	op_mproflinefetch
ENTRY op_mproflinefetch
	movl	frame_pointer,%eax
	popl	msf_mpc_off(%eax)
	call	gtm_fetch
	call	pcurrpos
	popl	%eax		# popping generated code args off stack before leak check
	leal	(%esp,%eax,4),%esp
	call	stack_leak_check
	movl	frame_pointer,%eax
	pushl	msf_mpc_off(%eax)
	ret
# op_mproflinefetch ENDP

# END
