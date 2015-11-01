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
	movl	frame_pointer,%edx
	movb	msf_typ_off(%edx),%al
	andb	$~SFT_EXTFUN,msf_typ_off(%edx)
	andb	$SFT_EXTFUN,%al
	je	l1
	ret

l1:	putframe
	pushl	ERR_ACTLSTEXP
	pushl	$1
	call	rts_error
	addl	$8,%esp
	ret
# op_isformal ENDP

# END
