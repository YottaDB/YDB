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
	.title	mint2mval.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.include	"mval_def.si"

	.sbttl	mint2mval
#	PAGE	+
	.text

# --------------------------------
# mint2mval.s
#	Convert int to mval
# --------------------------------

.extern	i2mval

# PUBLIC	mint2mval
ENTRY mint2mval
	movl	REG32_RET1,REG32_ARG1
        movq	REG64_RET0,REG64_ARG0
	call	i2mval
	ret
# mint2mval ENDP

# END
