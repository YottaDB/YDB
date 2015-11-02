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
	.title	opp_tstart.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_tstart
#	PAGE	+
	.DATA
.extern	frame_pointer 	# /* :DWORD */

	.text
.extern	op_tstart

# PUBLIC	opp_tstart
ENTRY opp_tstart  	# /* PROC */
	putframe
	movl	$0,%eax			# put arg0 over return call address since
	movl	%eax,(%esp)		# .. we dont need it: NOT an implicit tstart
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
