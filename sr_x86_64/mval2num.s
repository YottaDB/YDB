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
	.title	mval2num.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	mval2num
#	PAGE	+
	.text

.extern	n2s
.extern	s2n

# PUBLIC	mval2num
ENTRY mval2num
	mv_force_defined REG64_RET1, isdefined
	pushq   REG64_RET1                            # save in case call s2n
        mv_force_num REG64_RET1, l1
        popq    REG64_RET1
        mv_force_str_if_num_approx REG64_RET1, l2
	ret
# mval2num ENDP

# END
