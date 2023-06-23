#################################################################
#								#
# Copyright (c) 2001-2019 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#	PAGE	,132
	.title	op_restartpc.s
	.sbttl	op_restartpc

#	.386
#	.MODEL	FLAT, C

.include "linkage.si"
.include "g_msf.si"

	.DATA
.extern frame_pointer

	.text
# PUBLIC	op_restartpc
ENTRY op_restartpc
	movl	(%esp),%eax
	subl	$6,%eax
	movl	frame_pointer,%edx
	movl	%eax,msf_restart_pc_off(%edx)
	movl	msf_ctxt_off(%edx),%eax
	movl	%eax,msf_restart_ctxt_off(%edx)
	ret
# op_restartpc ENDP

# END
