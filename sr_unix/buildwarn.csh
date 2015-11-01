#!/usr/local/bin/tcsh
#
#################################################################
#								#
#	Copyright 2004 Sanchez Computer Associates, Inc.	#
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

set platform = `uname | sed 's/-/_/g' | tr '[A-Z]' '[a-z]'`
set host     = $HOST:r:r

pushd $gtm_ver/log

if (! -e comlist.$image.log) then
	echo "buildwarn-E-lognotexist, $gtm_ver/log/comlist.$image.log does not exist. Exiting..."
	exit -1
endif

# Generate warning file

set buildlog = comlist.$image.log
set warnlog  = warn.$image.log

switch ($platform)
	case "aix":
	case "sunos":
	case "osf1":
		if (-e $gtm_tools/buildwarn_$platform.awk) then
			awk -v gtm_src="$gtm_src" -f $gtm_tools/buildwarn_$platform.awk $buildlog > $warnlog
		else
			echo "buildwarn-E-awknotexist, $gtm_tools/buildwarn_$platform.awk does not exist. Exiting..."
			exit -1
		endif
		breaksw;
	case "linux":
		grep ":" $buildlog | grep "`echo $gtm_src`" > $warnlog
		breaksw;
	case "hp_ux":
		grep "cc:" $buildlog > $warnlog
		breaksw;
	default:
		echo "buildwarn-E-unsupport, Platform `uname` is unsupported currently. Exiting..."
		exit -1
		breaksw;
endsw

exit $status
