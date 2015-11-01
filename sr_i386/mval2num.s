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
	.title	mval2num.s

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	mval2num
#	PAGE	+
	.text

.extern	n2s
.extern	s2n

# PUBLIC	mval2num
ENTRY mval2num
	pushl	%edx				# save in case call s2n
	mv_force_num %edx, l1
	popl	%edx
	mv_force_str_if_num_approx %edx, l2
	ret
# mval2num ENDP

# END
