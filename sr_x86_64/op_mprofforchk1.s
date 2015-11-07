#################################################################
#								#
# Copyright (c) 2011-2015 Fidelity National Information 	#
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
	.extern forchkhandler

#
# This is the M profiling version which calls different routine(s) for M profiling purposes.
#
ENTRY	op_mprofforchk1
	movq    (REG_SP), REG64_ARG0		# Send return address to forchkhandler
	subq	$8, REG_SP			# Bump stack for 16 byte alignment
	CHKSTKALIGN				# Verify stack alignment
	call	forchkhandler
	addq	$8, REG_SP			# Remove stack alignment bump
	ret
