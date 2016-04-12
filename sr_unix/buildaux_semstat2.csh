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
echo "############# Linking SEMSTAT2 ###########"
echo ""
@ buildaux_semstat2_status = 0
source $gtm_tools/gtm_env.csh

set aix_loadmap_option = ''
if ( $HOSTOS == "AIX") then
	set aix_loadmap_option = "-bcalls:$gtm_map/semstat2.loadmap"
	set aix_loadmap_option = "$aix_loadmap_option -bmap:$gtm_map/semstat2.loadmap"
	set aix_loadmap_option = "$aix_loadmap_option -bxref:$gtm_map/semstat2.loadmap"
endif
set echo
gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/semstat2 -L$gtm_obj $gtm_obj/semstat2.o \
	$gt_ld_sysrtns $gt_ld_extra_libs $gt_ld_syslibs >& $gtm_map/semstat2.map
@ exit_status = $status
unset echo
if ( $exit_status != 0  ||  ! -x $3/semstat2 ) then
	@ buildaux_semstat2_status++
	echo "buildaux-E-linksemstat2, Failed to link semstat2 (see ${dollar_sign}gtm_map/semstat2.map)" \
		>> $gtm_log/error.${gtm_exe:t}.log
else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
	if ( "dbg" == $gt_image ) then
		chatr +dbg enable $3/semstat2
	endif
endif
exit $buildaux_semstat2_status
