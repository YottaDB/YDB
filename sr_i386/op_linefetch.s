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
	.title	op_linefetch.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_linefetch
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	gtm_fetch

# PUBLIC	op_linefetch
ENTRY op_linefetch
	movl	frame_pointer,%eax
	popl	msf_mpc_off(%eax)
	call	gtm_fetch
	popl	%eax
	leal	(%esp,%eax,4),%esp
	movl	frame_pointer,%eax
	pushl	msf_mpc_off(%eax)
	ret
# op_linefetch ENDP

# END
