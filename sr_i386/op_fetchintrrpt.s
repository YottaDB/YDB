#################################################################
#								#
#	Copyright 2001, 2009 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_fetchintrrpt.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_fetchintrrpt
#	PAGE	+
	.DATA
.extern	frame_pointer
.extern	neterr_pending

	.text
.extern	gtm_fetch
.extern	gvcmz_neterr
.extern	outofband_clear
.extern	async_action

# PUBLIC	op_fetchintrrpt
ENTRY op_fetchintrrpt
	movl	frame_pointer,%edx
	popl	msf_mpc_off(%edx)
	call	gtm_fetch
	popl	%eax
	leal	(%esp,%eax,4),%esp
	cmpb	$0,neterr_pending
	je	l1
	call	outofband_clear
	pushl	$0
	call	gvcmz_neterr
	addl	$4,%esp
l1:	pushl   $1
	call	async_action
	movl	frame_pointer,%edx
	pushl	msf_mpc_off(%edx)
	ret
# op_fetchintrrpt ENDP

# END
