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
	.title	op_sorts_after.s
	.sbttl	op_sorts_after

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"

# op_sorts_after.s 80386
#
# op_sorts_after(mval *mval1, *mval2)
#	Call sorts_after() to determine whether mval1 comes after mval2
#	in sorting order.  Use alternate local collation sequence if
#	present.
#
#	entry:
#		eax	mval *mval1
#		edx	mval *mval2
#
#	Sets condition flags and returns in eax:
##	1	mval1 > mval2
##	0	mval1 = mval2
##	-1	mval1 < mval2
#

	.text
.extern	sorts_after

# PUBLIC	op_sorts_after
ENTRY op_sorts_after
	movq	REG64_RET1,REG64_ARG1
	movq	REG64_RET0,REG64_ARG0
	call	sorts_after
	cmpl	$0,REG32_ACCUM		# set flags according to result from
	ret			# sorts_after.
# op_sorts_after ENDP

# END
