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
# Note: This script only works when called from buildaux.csh
#
set gt_image = $1
set gt_ld_options = "$2"

echo ""
echo "############# Linking GTMSECSHR ###########"
echo ""
@ buildaux_gtmsecshr_status = 0
source $gtm_tools/gtm_env.csh

set aix_loadmap_option = ''
$gtm_com/IGS $3/gtmsecshr "STOP"	# stop any active gtmsecshr processes
$gtm_com/IGS $3/gtmsecshr "RMDIR"	# remove root-owned gtmsecshr, gtmsecshrdir, gtmsecshrdir/gtmsecshr files/dirs
foreach file (gtmsecshr gtmsecshr_wrapper)
	if ( $HOSTOS == "AIX") then
	    set aix_loadmap_option = "-bcalls:$gtm_map/$file.loadmap"
	    set aix_loadmap_option = "$aix_loadmap_option -bmap:$gtm_map/$file.loadmap"
	    set aix_loadmap_option = "$aix_loadmap_option -bxref:$gtm_map/$file.loadmap"
	endif
	set echo
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/${file} -L$gtm_obj $gtm_obj/${file}.o \
			$gt_ld_sysrtns $gt_ld_extra_libs -lmumps $gt_ld_syslibs >& $gtm_map/${file}.map
	@ exit_status = $status
	unset echo
	if ( $exit_status != 0  ||  ! -x $3/${file} ) then
		@ buildaux_gtmsecshr_status++
		echo "buildaux-E-link${file}, Failed to link ${file} (see ${dollar_sign}gtm_map/${file}.map)" \
			>> $gtm_log/error.${gtm_exe:t}.log
	else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
		if ( "dbg" == $gt_image ) then
			chatr +dbg enable +as mpas $3/${file}
		else
			chatr +as mpas $3/${file}
		endif
	endif
end
mkdir ../gtmsecshrdir
mv ../gtmsecshr ../gtmsecshrdir	  	# move actual gtmsecshr into subdirectory
mv ../gtmsecshr_wrapper ../gtmsecshr	  # rename wrapper to be actual gtmsecshr

# add symbolic link to gtmsecshrdir in utf8 if utf8 exists
if ( -d utf8 ) then
	cd utf8; ln -s ../gtmsecshrdir gtmsecshrdir; cd -
endif
$gtm_com/IGS $3/gtmsecshr "CHOWN" # make gtmsecshr, gtmsecshrdir, gtmsecshrdir/gtmsecshr files/dirs root owned
if ($status) @ buildaux_gtmsecshr_status++
exit $buildaux_gtmsecshr_status
