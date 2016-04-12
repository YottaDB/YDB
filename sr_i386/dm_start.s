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
	.title	dm_start.s
	.sbttl	dm_start

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.include "error.si"

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

.ifndef cygwin
 	.type dm_start,@function
.endif
ENTRY dm_start
	enter	$0,$0
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	movl	$1,mumps_status
	leal	xfer_table,%ebx
	movl	$1,dollar_truth
	ESTABLISH mdb_condition_handler, l30
	call	*restart

return:	movl	mumps_status,%eax
	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret

ENTRY gtm_ret_code
	REVERT
	call	op_unwind
	movl	msp,%eax
	movl	(%eax),%eax
	movl	%eax,frame_pointer
	addl	$4,msp
	jmp	return

# Used by triggers (and eventually call-ins) to return from a nested generated code call
# (a call not at the base C stack level).
ENTRY gtm_levl_ret_code
	REVERT
	jmp	return

# dm_start ENDP

# END
