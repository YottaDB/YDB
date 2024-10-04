#################################################################
#								#
# Copyright (c) 2007-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

	.include "linkage.si"
	.include "g_msf.si"
	.include "mval_def.si"
#	include "debug.si"

# args:
#	See op_pattern_retbool.c for input args details
#
	.data

	.text
	.extern	do_patfixed
	.extern	do_pattern

ENTRY	opp_pattern_retbool
	subq	$8, %rsp			# Bump stack for 16 byte alignment
	CHKSTKALIGN				# Verify stack alignment
	movw	mval_w_mvtype(%rdi), %r11w
	testw	$mval_m_sqlnull, %r11w		# See if $ZYSQLNULL
	jne	sqlnull				# If yes, then return FALSE

	movq	mval_a_straddr(%rsi), %rax
	#
	# This is an array of unaligned ints. If the first word is zero, then call do_pattern
	# instead of do_patfixed. Only the low order byte is significant and so it is the only
	# one we need to test. We would do this in assembly because (1) we need the assmembly
	# routine anyway to set up the condition code the generated code needs and (2) it
	# saves an extra level of call linkage at the C level to do the decision here.
	#
	cmpb	$0, 0(%rax)				# Little endian compare of low order byte
	je	l1
	call	do_patfixed

done:
	addq	$8, %rsp				# Remove stack alignment bump
	cmpl	$0, %eax				# Set condition code for caller
	ret

l1:
	call	do_pattern
	jmp	done

sqlnull:
	movl	$0, %eax
	jmp	done

# Below line is needed to avoid the ELF executable from ending up with an executable stack marking.
# This marking is not an issue in Linux but is in Windows Subsystem on Linux (WSL) which does not enable executable stack.
.section        .note.GNU-stack,"",@progbits