#################################################################
#								#
# Copyright (c) 2001-2018 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
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
#	buildpro.csh - Build pro images.
#
#	Argument:
#		$1 -	Version number or code (i.e., b, d, or p).
#
##################################################################

if ( $1 == "" ) then
	echo "buildpro-E-needp1, Usage: $shell buildpro.csh <version>"
	exit -1
endif

version $1 p
$gtm_tools/buildbdp.csh $1 pro $gtm_vrt/pro
# Extract the debug symbols from each executable
if ( "$HOSTOS" == "Linux" ) then
	rm stripping_log.txt >& /dev/null
	echo "Stripping debug symbols and generating .debug files. Leaving log at `pwd`/stripping_log.txt"
	foreach file (`find ../ -executable -type f`)
		echo "Stripping $file"
		objcopy --only-keep-debug $file $file.debug >> stripping_log.txt
		strip -g $file >> stripping_log.txt
		objcopy --add-gnu-debuglink=$file.debug $file >> stripping_log.txt
	end
endif

exit $status
