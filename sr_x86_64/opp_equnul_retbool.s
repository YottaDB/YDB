#################################################################
#								#
# Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Portions Copyright (c) Fidelity National			#
# Information Services, Inc. and/or its subsidiaries.		#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# This file is based on sr_x86_64/op_equnul.s (before it was deleted in 5b1b5b1b)
# hence the inclusion of a FIS copyright too.

# This is an assembly implementation of "sr_port/op_equnul_retbool.c".
# Done currently only on x86_64 for performance reasons (on aarch64 and armv7l,
# opp_equnul_retbool.s simply calls into op_equnul_retbool.c).

	.include "g_msf.si"
	.include "linkage.si"
	.include "mval_def.si"
#	include "debug.si"

	.data
	.extern	undef_inhibit

	.text
	.extern	underr

#
# Routine to compare input mval (passed in %rdi) to see if it is the NULL string. Primary return is setting
# the condition code for a conditional branch in the caller. If caller wants/needs it, the return (int) value is
# also set.
#
ENTRY	opp_equnul_retbool
	subq	$8, %rsp				# Bump stack for 16 byte alignment
	CHKSTKALIGN					# Verify stack alignment
	movw	mval_w_mvtype(%rdi), %r11w
	testw	$mval_m_str, %r11w			# See if a string
	je	notstr					# If not a string, then not a null string
	testw	$mval_m_sqlnull, %r11w			# See if $ZYSQLNULL
	jne	notnullstr				# If yes, then treat it as NOT equal to the null string
	cmpl	$0, mval_l_strlen(%rdi)			# Verify string length is 0
	jne	notnullstr				# If not, not a null string
nullstr:
	#
	# We have a null string. Return value not really used but set it for comparison purposes
	#
	movl	$1, %eax
	jmp	done
notnullstr:
	#
	# We have either a non-null string or not a string at all.
	#
	movl	$0, %eax
done:
	addq	$8, %rsp				# Remove stack alignment bump
	cmpl	$0, %eax				# Set condition code for caller
	ret
notstr:
	testw	$mval_m_nm, %r11w			# See if a number
	jne	notnullstr				# If yes then it is not the null string

	# If we reach here, it means the input was not a string or a number. That is, it is undefined.
	# Here when input mval not defined. If undef_inhibit set, make the assumption the value is the NULL string.
	# Else, raise the undef error.
	#
undefmval:
	cmpb	$0, undef_inhibit(%rip)			# Test undef_inhibit setting
	jne	nullstr					# It's set, return as if was NULL
	movb    $0, %al					# Variable length argumentt
	call	underr					# Give undef error
	jmp	done					# Should never return but if do - at least return

