#################################################################
#								#
#	Copyright 2001, 2010 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_retarg.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_retarg
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	unw_retarg

# PUBLIC	op_retarg
ENTRY op_retarg
	movl	%edx,(%esp)	# Reuse return point on stack (not needed)
	pushl	%eax
	call	unw_retarg
	addl	$8,%esp
	getframe
	ret
# op_retarg ENDP

# END
