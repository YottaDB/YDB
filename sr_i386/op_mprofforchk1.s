#################################################################
#								#
#	Copyright 2011 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

	.title	op_mprofforchk1.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_mprofforchk1
#	Called with the stack contents:
#		call return
	.DATA

	.text
.extern	forchkhandler

# PUBLIC	op_mprofforchk1
ENTRY op_mprofforchk1

	pushl	0(%esp)		# throw the current return address on as an arg
	call	forchkhandler
	addl	$4, %esp
	ret

# op_mprofforchk1 ENDP

# END
