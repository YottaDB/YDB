#!/usr/local/bin/tcsh -f
#################################################################
#								#
# Copyright (c) 2009-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
###########################################################################################
#
#	check_trigger_support.csh - Checks if triggers are available on a platform.
#				    There must the gtm_trigger.h for this source base to
#				    have triggers enabled
#	Returns :
#		TRUE - if the platform supports triggers (true for all posix platforms)
#		FALSE - if not supported
###########################################################################################

# HP-UX PA-RISC is not supported
if ( "HP-UX" == "$HOSTOS" && "ia64" != `uname -m` ) then
	echo "FALSE"
	exit 0
endif

ls $gtm_inc/gtm_trigger.h >& /dev/null && echo "TRUE" || echo "FALSE"

exit 0
