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
	.title	op_forlcldo.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_forlcldo
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	exfun_frame

	.sbttl	op_forlcldob
# PUBLIC	op_forlcldob
ENTRY op_forlcldob
	movl	frame_pointer,%edx
	movl	(%esp),%eax
	movl	%eax,msf_mpc_off(%edx)
	addl	$2,msf_mpc_off(%edx)	# store pc in MUMPS stack frame
doit:	call	exfun_frame
	movl	frame_pointer,%edx
	movl	msf_temps_ptr_off(%edx),%edi
	ret
# op_forlcldob ENDP

	.sbttl	op_forlcldow, op_forlcldol
# PUBLIC	op_forlcldow, op_forlcldol
ENTRY op_forlcldol
ENTRY op_forlcldow
	movl	frame_pointer,%edx
	movl	(%esp),%eax
	movl	%eax,msf_mpc_off(%edx)
	addl	$5,msf_mpc_off(%edx)	# store pc in MUMPS stack frame
	jmp	doit
# op_forlcldol ENDP

# END
