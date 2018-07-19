#################################################################
#								#
# Copyright (c) 2018 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
        .include "linkage.si"
        .include "g_msf.si"
        .include "debug.si"

        .text
        .extern ci_ret_code_exit

ENTRY	opp_ciret
	CHKSTKALIGN
	call    ci_ret_code_exit
	ret
