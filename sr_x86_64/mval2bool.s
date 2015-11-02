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
	.title	mval2bool.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	mval2bool
#	PAGE	+
# --------------------------------
# mval2bool.s
#	Convert mval to bool
# --------------------------------
#	edx - src. mval

	.text
.extern	s2n

# PUBLIC	mval2bool
ENTRY mval2bool
	mv_force_defined REG_RET1, isdefined
	pushq	REG_RET1
	mv_force_num REG_RET1, skip_conv
	popq    REG_RET1
	cmpl    $0,mval_l_m1(REG_RET1)		#set condition of flag refgister
	ret
# mval2bool ENDP

# END
