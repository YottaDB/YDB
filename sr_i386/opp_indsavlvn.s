#################################################################
#								#
#	Copyright 2012 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	opp_indsavlvn.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_indsavlvn
#	PAGE	+
	.DATA
.extern	frame_pointer 	# /* :DWORD */

	.text
.extern	op_indsavlvn

# PUBLIC	opp_indsavlvn
ENTRY opp_indsavlvn  	# /* PROC */
	putframe
	addl	$4,%esp		# /* burn return pc */
	call	op_indsavlvn
	addl	$8,%esp		# /* burn two passed-in args */
	getframe
	ret
# opp_indsavlvn ENDP

# END
