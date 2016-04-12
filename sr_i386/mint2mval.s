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
	.title	mint2mval.s

#	.386
#	.MODEL	FLAT, C

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
	pushl	%edx
	leal	(%eax),%eax
	pushl	%eax
	call	i2mval
	addl	$8,%esp
	ret
# mint2mval ENDP

# END
