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
	.title	ci_restart.s
	.sbttl	ci_restart
#	.386
#	.MODEL	FLAT, C
.include "linkage.si"

	.DATA
.extern	param_list

	.text
.extern	ci_ret_code

ENTRY ci_restart
	movl	param_list,%eax
	movl 	4(%eax),%eax			# argcnt
	cmpl 	$0,%eax				# if (argcnt > 0) {
	jle 	L0
	imull	$4,%eax,%edx			# param_list->args[argcnt]
	leal    20(%edx),%edx
	pushl	%eax
	movl	param_list,%eax
	addl	%eax,%edx
	popl	%eax
L1:	pushl 	0(%edx)				# pushing arguments backwards
	subl	$4,%edx
	subl	$1,%eax
	cmpl	$0,%eax
	jg	L1				# }
L0:	movl	param_list,%eax
	pushl 	4(%eax)				#argcnt
	pushl 	20(%eax)			#mask
	pushl 	16(%eax)			#retaddr
	pushl 	12(%eax)			#labaddr
	pushl 	8(%eax)				#rtnaddr
	call	*(%eax)
	call	ci_ret_code
	ret

# ci_restart ENDP

# END
