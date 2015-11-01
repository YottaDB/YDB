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
	.title	opp_tstart.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_tstart
#	PAGE	+
	.DATA
.extern	dollar_truth 	# /* :DWORD */
.extern	frame_pointer 	# /* :DWORD */

	.text
.extern	op_tstart

# PUBLIC	opp_tstart
ENTRY opp_tstart  	# /* PROC */
	putframe
	movl	dollar_truth,%eax	# put $T over return call address,
	movl	%eax,(%esp)		# we dont need it
	call	op_tstart
	movl	12(%esp),%eax		# get number of variables to preserve
	cmpl	$0,%eax			# -1 = not restartable,
	jge	l1			# -2 = preserve all variables
	movl	$0,%eax
l1:	addl	$4,%eax			# total args to op_tstart == preservecnt + 4
	leal	(%esp,%eax,4),%esp
	getframe
	ret
# opp_tstart ENDP

# END
