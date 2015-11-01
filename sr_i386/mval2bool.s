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
	.title	mval2bool.s

#	.386
#	.MODEL	FLAT, C

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
	pushl	%edx
	mv_force_num %edx, skip_conv
	popl	%edx
	cmpl	$0,mval_l_m1(%edx)
	ret
# mval2bool ENDP

# END
