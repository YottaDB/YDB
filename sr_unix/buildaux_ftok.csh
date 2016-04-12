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
echo "############# Linking FTOK ###########"
echo ""
@ buildaux_ftok_status = 0
source $gtm_tools/gtm_env.csh

set aix_loadmap_option = ''
if ( $HOSTOS == "AIX") then
	set aix_loadmap_option = "-bcalls:$gtm_map/ftok.loadmap -bmap:$gtm_map/ftok.loadmap -bxref:$gtm_map/ftok.loadmap"
endif
set echo
gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/ftok -L$gtm_obj $gtm_obj/ftok.o \
		$gt_ld_sysrtns -lmupip -lmumps -lstub $gt_ld_extra_libs $gt_ld_syslibs >& $gtm_map/ftok.map
@ exit_status = $status
unset echo
if ( $exit_status != 0  ||  ! -x $3/ftok ) then
	@ buildaux_ftok_status++
	echo "buildaux-E-linkftok, Failed to link ftok (see ${dollar_sign}gtm_map/ftok.map)" \
		>> $gtm_log/error.${gtm_exe:t}.log
else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
	if ( "dbg" == $gt_image ) then
		chatr +dbg enable $3/ftok
	endif
endif
exit $buildaux_ftok_status
