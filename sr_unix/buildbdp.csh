#################################################################
#								#
#	Copyright 2001, 2008 Fidelity Information Services, Inc	#
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

$shell $gtm_tools/buildshr.csh $1 $2 $3
if ($status != 0) @ buildbdp_status = $status

$shell $gtm_tools/buildaux.csh $1 $2 $3
if ($status != 0) @ buildbdp_status = $status

exit $buildbdp_status
