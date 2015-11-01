#################################################################
#								#
#	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	#
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
	movw	msf_typ_off(%edx),%ax
	andw	$~SFT_EXTFUN,msf_typ_off(%edx)
	andw	$SFT_EXTFUN,%ax
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
