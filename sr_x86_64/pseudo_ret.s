#################################################################
#								#
#	Copyright 2007 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	pseudo_ret.s
	.sbttl	pseudo_ret

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"

	.text
.extern	opp_ret

# PUBLIC	pseudo_ret
ENTRY pseudo_ret
	call	opp_ret
# pseudo_ret ENDP

# END
