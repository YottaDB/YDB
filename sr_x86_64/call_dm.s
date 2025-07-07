#################################################################
#								#
# Copyright (c) 2007-2025 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

	.include "g_msf.si"
	.include "linkage.si"
	.include "debug.si"

	.text
	.extern	op_oldvar
	.extern	opp_dmode

#
# Note call_dm is only ever branched to so does not have a return address pushed on the stack throwing off the
# needed 16 byte alignment of the stack. Verify that first thing on each iteration.
#
ENTRY	call_dm
newcmd:
	CHKSTKALIGN			# Verify stack alignment
	call	opp_dmode
	jmp	newcmd
