#################################################################
#								#
#	Copyright 2007, 2008 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	mval2mint.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	mval2mint
#	PAGE	+
# --------------------------------
# mval2mint.s
#	Convert mval to int
# --------------------------------
#	edx - source mval
#	eax - destination mval

	.text
.extern	mval2i
.extern	s2n

# PUBLIC	mval2mint
ENTRY mval2mint
	mv_force_defined REG64_RET1, isdefined
	pushq	REG64_RET1
        mv_force_num REG64_RET1, skip_conv
	popq	REG64_ARG0
	call	mval2i
	ret
# mval2mint ENDP

# END
