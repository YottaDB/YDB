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
	.title	op_mproflinestart.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_mproflinestart
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	pcurrpos

MPR_LINESTART	=	0x8

# PUBLIC	op_mproflinestart
ENTRY op_mproflinestart
	putframe
	pushl	$MPR_LINESTART
	call	pcurrpos
	addl 	$4,%esp
	getframe
	movl	frame_pointer,%edx
	movl	(%esp),%eax
	movl	%eax,msf_mpc_off(%edx)
	ret
# op_mproflinestart ENDP

# END
