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
	.title	op_zhelp.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE "g_msf.si"

#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	op_zhelp_xfr

# PUBLIC	op_zhelp
ENTRY op_zhelp
	movq	frame_pointer(REG_IP),REG64_RET1
	popq	msf_mpc_off(REG64_RET1)
	call	op_zhelp_xfr
	getframe
	ret
# op_zhelp ENDP

# END
