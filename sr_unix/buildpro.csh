#################################################################
#								#
# Copyright (c) 2001-2020 Fidelity National Information		#
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

set setactive_parms = ( $1 p ) ; source $gtm_tools/setactive.csh
$gtm_tools/buildbdp.csh $1 pro $gtm_ver/pro
set buildstatus=$status

# Extract the debug symbols from each executable
if ( "$HOSTOS" == "Linux" ) then
	set outfile = "strip_debug_symbols.out"
	rm -f $outfile
	echo "Stripping debug symbols and generating .debug files. Leaving log at $PWD/$outfile"
	foreach file (`find ../ -executable -type f`)
		if ($file:e =~ {sh,csh}) continue
		echo "Stripping $file"				>>&! $outfile
		objcopy --only-keep-debug $file $file.debug	>>&! $outfile
		strip -g $file					>>&! $outfile
		objcopy --add-gnu-debuglink=$file.debug $file	>>&! $outfile
	end
endif

# strip removes the restricted permissions of gtmsecshr. Fix it
$gtm_com/IGS $gtm_dist/gtmsecshr CHOWN

exit $buildstatus
