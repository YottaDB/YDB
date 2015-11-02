#################################################################
#								#
#	Copyright 2007, 2012 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_fnzextract.s

#	.386
#	.MODEL	FLAT, C

.include "g_msf.si"
.include "linkage.si"
	.INCLUDE	"mval_def.si"

	.sbttl	op_fnzextract
#	PAGE	+
# ------------------------------------
# op_fnzextract.s
#
# Mumps $Extract function
# ------------------------------------

# --------------------------------
#	op_fnzextract (int last, int first, mval *src, mval *dest)
# --------------------------------
last    =       -12
first	=	-16
src	=	-24
dest	=	-32

	.text
.extern	n2s

# PUBLIC	op_fnzextract
ENTRY op_fnzextract
	pushq	REG_XFER_TABLE
	enter	$48,$0    # Need to make sure that the SP will be 16 bytes aligned
	movl	REG32_ARG0,last(REG_FRAME_POINTER)
	movl	REG32_ARG1,first(REG_FRAME_POINTER)
	movq	REG64_ARG3,dest(REG_FRAME_POINTER)
	mv_force_defined REG64_ARG2, l00
	movq	REG64_ARG2,src(REG_FRAME_POINTER)
	mv_force_str REG64_ARG2,l01
	movq	src(REG_FRAME_POINTER),REG64_ARG1
	movl	first(REG_FRAME_POINTER),REG32_ACCUM
	cmpl	$0,REG32_ACCUM
	jg	l10
	movl	$1,REG32_ACCUM			# if first < 1, then first = 1
l10:	movl	last(REG_FRAME_POINTER),REG32_ARG2
	movq	dest(REG_FRAME_POINTER),REG64_ARG0
	movw	$mval_m_str,mval_w_mvtype(REG64_ARG0)
	movl	mval_l_strlen(REG64_ARG1),REG32_ARG3
	cmpl	REG32_ACCUM,REG32_ARG3		# if left index > str. len,
						# then null result
	jl	l25
	cmpl	REG32_ARG2,REG32_ARG3		# right index may be at most the len.
	jge	l20			# of the source string
	movl	REG32_ARG3,REG32_ARG2
l20:	movl	REG32_ARG2,REG32_SCRATCH1
	subl	REG32_ACCUM,REG32_SCRATCH1		# result len. = end - start + 1
	addl	$1,REG32_SCRATCH1
	jg	l30			# if len > 0, then continue
l25:	movl	$0,mval_l_strlen(REG64_ARG0)
	jmp	retlab

l30:	movl 	REG32_SCRATCH1,mval_l_strlen(REG64_ARG0)
	subl	$1,REG32_ACCUM				# base = src.addr + left ind. - 1
	addq	mval_a_straddr(REG64_ARG1),REG64_ACCUM
	movq	REG64_ACCUM,mval_a_straddr(REG64_ARG0)
retlab:
	leave
	popq	REG_XFER_TABLE
	ret
# op_fnzextract ENDP

# END
