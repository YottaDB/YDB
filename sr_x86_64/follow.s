#################################################################
#								#
# Copyright (c) 2007-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

	.include "g_msf.si"
	.include "linkage.si"
	.include "debug.si"

	.text
	.extern	op_follow

ENTRY	follow
	subq	$8, REG_SP		# Align to 16 bytes
	CHKSTKALIGN			# Verify stack alignment
	movq	REG64_ARG0, REG64_RET0
	movq	REG64_ARG1, REG64_RET1
	call	op_follow
	jle	notfollow
	movq	$1, REG64_RET0
	jmp	done
notfollow:
	movq	$0, REG64_RET0
done:
	addq	$8, REG_SP		# Remove stack extension
	ret
