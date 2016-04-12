#################################################################
#								#
# Copyright (c) 2001-2015 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
##########################################################################
#
#	gt_ar.csh - Invoke library archiver.
#	This script is intended to be used with the xargs command, because
#	xargs requires any command it executes to be specified by full
#	pathname or to be on $PATH and will not handle an alias properly.
#
#	Arguments
#		Options and arguments to the archiver.
#
#	Aliases
#		gt_ar	invoke native archiver
#
##########################################################################

source $gtm_tools/gtm_env.csh
gt_ar $*
