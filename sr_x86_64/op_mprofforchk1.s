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

#	PAGE	,132
	.title	op_mprofforchk1.s
	.sbttl	op_mprofforchk1

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"

	.text
.extern forchkhandler

# PUBLIC	op_mprofforchk1
ENTRY op_mprofforchk1
	movq    (REG_SP),REG64_ARG0	# Send return address to forchkhandler
	call	forchkhandler
	ret
# op_mprofforchk1 ENDP

# END
