#################################################################
#								#
#	Copyright 2001, 2008 Fidelity Information Services, Inc	#
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
.extern underr

# PUBLIC	mval2mint
ENTRY mval2mint
	mv_force_defined %edx, l1
	pushl	%edx			# preserve src + push it as arg
	mv_force_num %edx, skip_conv
	call	mval2i
	addl	$4,%esp
	ret
# mval2mint ENDP

# END
