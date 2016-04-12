#!/usr/local/bin/tcsh -f
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
#################################################################################
#
#	buildbdp.csh - Build (link) b (bta), d (dbg), or p (pro) version of GT.M.
#
#	Arguments:
#		$1 -	Version number of release.
#		$2 -	Image type (b[ta], d[bg], or p[ro]).
#		$3 -	Pathname of directory for executables.
#
#################################################################################

set buildbdp_status = 0

if ( "$1" == "" ) then
	set buildbdp_status = -1
endif

if ( "$2" == "" ) then
	set buildbdp_status = -1
endif

if ( $buildbdp_status != 0 ) then
	echo "buildbdp-I-needp1orp2, Usage: $shell $gtm_tools/buildbdp.csh <version> <directory for images>"
	exit $buildbdp_status
endif

$gtm_tools/buildaux.csh $1 $2 $3 "shr"
if ($status != 0) @ buildbdp_status = $status

exit $buildbdp_status
