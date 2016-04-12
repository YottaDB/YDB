#!/usr/local/bin/tcsh
#
#################################################################
#								#
#	Copyright 2004, 2007 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
##################################################################
#
#	buildwarn.csh - Filter out all the warnings from the build logs
#
#	Argument:
#		$1 -	version number
#		$2 -	image type (b[ta], d[bg], or p[ro])
#
##################################################################

set buildwarn_status = 0

if ( $1 == "" ) then
	@ buildwarn_status++;
endif

if ( $2 == "" ) then
	@ buildwarn_status++;
endif

switch ($2)
	case "[bB]*":
		set image = "bta"
		breaksw
	case "[dD]*":
		set image = "dbg"
		breaksw
	case "[pP]*":
		set image = "pro"
		breaksw
	default:
		set image = ""
		@ buildwarn_status++;
		breaksw
endsw

if (! -e $gtm_root/$1/$image) then
	@ buildwarn_status++;
endif

if ( $buildwarn_status != 0 ) then
	echo "buildwarn-E-needp12, Usage: $gtm_tools/buildwarn.csh <version> <image type>"
	exit $buildwarn_status
endif

version $1 $2

pushd $gtm_ver/log

if (! -e comlist.$image.log) then
	echo "buildwarn-E-lognotexist, $gtm_ver/log/comlist.$image.log does not exist. Exiting..."
	exit -1
endif

# Generate warning file

set buildlog = comlist.$image.log
set warnlog  = warn.$image.log

awk -v gtm_src="$gtm_src" -f $gtm_tools/buildwarn.awk $buildlog > $warnlog
exit $status
