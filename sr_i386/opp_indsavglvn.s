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
	.title	opp_indsavglvn.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	opp_indsavglvn
#	PAGE	+
	.DATA
.extern	frame_pointer 	# /* :DWORD */

	.text
.extern	op_indsavglvn

# PUBLIC	opp_indsavglvn
ENTRY opp_indsavglvn  	# /* PROC */
	putframe
	addl	$4,%esp		# /* burn return pc */
	call	op_indsavglvn
	addl	$12,%esp	# /* burn three passed-in args */
	getframe
	ret
# opp_indsavglvn ENDP

# END
