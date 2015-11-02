#################################################################
#								#
#	Copyright 2001, 2012 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_mprofforlcldo.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_mprofforlcldo
#	PAGE	+
	.DATA
.extern	frame_pointer

	.text
.extern	exfun_frame_sp

	.sbttl	op_mprofforlcldob
# PUBLIC	op_mprofforlcldob
ENTRY op_mprofforlcldob
	movl	frame_pointer,%edx
	movl	(%esp),%eax
	movl	%eax,msf_mpc_off(%edx)
	addl	$2,msf_mpc_off(%edx)	# store pc in MUMPS stack frame
doit:	call	exfun_frame_sp
	movl	frame_pointer,%edx
	movl	msf_temps_ptr_off(%edx),%edi
	ret
# op_mprofforlcldob ENDP

	.sbttl	op_mprofforlcldow, op_mprofforlcldol
# PUBLIC	op_mprofforlcldow, op_mprofforlcldol
ENTRY op_mprofforlcldol
ENTRY op_mprofforlcldow
	movl	frame_pointer,%edx
	movl	(%esp),%eax
	movl	%eax,msf_mpc_off(%edx)
	addl	$5,msf_mpc_off(%edx)	# store pc in MUMPS stack frame
	jmp	doit
# op_mprofforlcldol ENDP

# END
