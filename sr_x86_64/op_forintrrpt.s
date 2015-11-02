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
	.title	op_forintrrpt.s
	.sbttl	op_forintrrpt

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"

	.DATA
.extern	neterr_pending
.extern	restart_pc

	.text
.extern	gvcmz_neterr
.extern	async_action
.extern	outofband_clear

# PUBLIC	op_forintrrpt
ENTRY op_forintrrpt
	cmpb	$0,neterr_pending(REG_IP)
	je	l1
	call	outofband_clear
	movq	$0,REG64_ARG0
	call	gvcmz_neterr
l1:	movl	$0,REG32_ARG0
	call	async_action
	ret
# op_forintrrpt ENDP

# END
