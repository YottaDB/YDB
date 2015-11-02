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
	.title	op_startintrrpt.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_startintrrpt
#	PAGE	+
	.DATA
.extern	frame_pointer
.extern	neterr_pending

	.text
.extern	gvcmz_neterr
.extern	async_action
.extern	outofband_clear

# PUBLIC	op_startintrrpt
ENTRY op_startintrrpt
	putframe
	cmpb	$0,neterr_pending(REG_IP)
	je	l1
	call	outofband_clear
	movq	$0,REG64_ARG0
	call	gvcmz_neterr
l1:	movl	$1,REG32_ARG0
	call	async_action
	addq	$8,REG_SP		# 8 bytes to burn return PC
	getframe
	ret
# op_startintrrpt ENDP

# END
