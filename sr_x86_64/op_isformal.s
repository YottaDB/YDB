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
	.title	op_isformal.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"g_msf.si"

	.sbttl	op_isformal
#	PAGE	+
	.DATA
.extern	ERR_ACTLSTEXP
.extern	frame_pointer

	.text
.extern	rts_error

# PUBLIC	op_isformal
ENTRY op_isformal
	movq	 frame_pointer(REG_IP),REG64_ARG2
        movw	 msf_typ_off(REG64_ARG2),REG16_ACCUM
        andw	$~SFT_EXTFUN,msf_typ_off(REG64_ARG2)
	andw	$SFT_EXTFUN,REG16_ACCUM
	je	l1
	ret

l1:	putframe
	movl	ERR_ACTLSTEXP(REG_IP),REG32_ARG1
	movl	$1,REG32_ARG0
	movb    $0,REG8_ACCUM             # variable length argument
	call	rts_error
	ret
# op_isformal ENDP

# END
