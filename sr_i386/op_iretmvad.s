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
	.title	op_iretmvad.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_iretmvad
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	op_unwind

# PUBLIC	op_iretmvad
ENTRY op_iretmvad
	movl	%edx,%ecx	# save input parameter from putframe macro
	putframe
	addl	$4,%esp
	movl	%ecx,%edx
	pushl	%edx
	call	op_unwind
	popl	%eax		# return input parameter
	getframe
	ret
# op_iretmvad ENDP

# END
