#################################################################
#								#
#	Copyright 2006, 2008 Fidelity Information Services, Inc	#
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
#	esi - src mval
#	edi - dest mval
#	ecx - src. str. len.
#	ebx - resultant str. len.
#	eax - first index
#	edx - last index

last	=	8
first	=	12
src	=	16
dest	=	20

	.text

.extern	n2s
.extern underr

# PUBLIC	op_fnzextract
ENTRY op_fnzextract
	enter	$0,$0
	pushl	%edi
	pushl	%esi
	pushl	%ebx

	movl	src(%ebp),%esi		# esi - src. mval
	mv_force_defined %esi, l00
	movl	%esi, src(%ebp)		# save possibly modified src ptr
	mv_force_str %esi, l01
	movl	src(%ebp),%esi		# esi - src.mval
	movl	first(%ebp),%eax	# eax - first
	cmpl	$0,%eax
	jg	l10
	movl	$1,%eax			# if first < 1, then first = 1
l10:	movl	last(%ebp),%edx		# edx - last
	movl	dest(%ebp),%edi		# edi - last
	movw	$mval_m_str,mval_w_mvtype(%edi)	# always a string
	movl	mval_l_strlen(%esi),%ecx	# ecx - src. str. len.
	cmpl	%eax,%ecx		# if left index > str. len,
						# then null result
	jl	l25
	cmpl	%edx,%ecx		# right index may be at most the len.
	jge	l20			# of the source string
	movl	%ecx,%edx
l20:	movl	%edx,%ebx
	subl	%eax,%ebx		# result len. = end - start + 1
	addl	$1,%ebx
	jg	l30			# if len > 0, then continue
l25:	movl	$0,mval_l_strlen(%edi)
	jmp	retlab

l30:	movl	%ebx,mval_l_strlen(%edi)	# dest. str. len.
	subl	$1,%eax				# base = src.addr + left ind. - 1
	addl	mval_a_straddr(%esi),%eax
	movl	%eax,mval_a_straddr(%edi)	# string addr.
retlab:	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret
# op_fnzextract ENDP

# END
