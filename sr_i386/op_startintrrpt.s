#################################################################
#								#
#	Copyright 2001 Sanchez Computer Associates, Inc.	#
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
	cmpb	$0,neterr_pending
	je	l1
	call	outofband_clear
	pushl	$0
	call	gvcmz_neterr
	addl	$4,%esp
l1:	pushl	$1
	call	async_action
	addl    $8,%esp # 4 bytes to burn return pc, 4 more to remove arg to async_action
	getframe
	ret
# op_startintrrpt ENDP

# END
