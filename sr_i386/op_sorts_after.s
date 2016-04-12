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
	.title	op_sorts_after.s
	.sbttl	op_sorts_after

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"

# op_sorts_after.s 80386
#
# op_sorts_after(mval *mval1, *mval2)
#     Call sorts_after() to determine whether mval1 comes after mval2
#     in sorting order.  Use alternate local collation sequence if
#     present.
#
#	entry:
#		eax	mval *mval1
#		edx	mval *mval2
#
#   Sets condition flags and returns in eax:
##          1     mval1 > mval2
##          0     mval1 = mval2
##         -1     mval1 < mval2
#

	.text
.extern	sorts_after

# PUBLIC	op_sorts_after
ENTRY op_sorts_after
	pushl	%edx
	pushl	%eax
	call	sorts_after
	addl	$8,%esp		# restore stack
	cmpl	$0,%eax		# set flags according to result from
	ret			# sorts_after.
# op_sorts_after ENDP

# END
