#################################################################
#								#
#	Copyright 2007, 2012 Fidelity Information Services, Inc	#
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
ENTRY op_mprofforlcldol
ENTRY op_mprofforlcldow
	movq    frame_pointer(REG_IP),REG64_ARG2
        movq    (REG_SP),REG64_ACCUM
        movq    REG64_ACCUM,msf_mpc_off(REG64_ARG2)
        addq    REG64_ARG0,msf_mpc_off(REG64_ARG2)
	call	exfun_frame_sp
	movq    frame_pointer(REG_IP),REG64_ARG2
        movq    msf_temps_ptr_off(REG64_ARG2),REG_FRAME_TMP_PTR
	ret
# op_mprofforlcldob ENDP
