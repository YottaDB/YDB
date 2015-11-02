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
	.title	call_dm.s
	.sbttl	call_dm
.include "g_msf.si"
.include "linkage.si"

	.DATA
	.text
.extern	op_oldvar
.extern	opp_dmode

# PUBLIC	call_dm
# call_dm	PROC
ENTRY call_dm
l1:	call	opp_dmode
	call	op_oldvar
	jmp	l1
	ret
# END
