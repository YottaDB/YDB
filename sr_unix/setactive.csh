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
#######################################################################################################################
#
#	source setactive.csh <version> <type> - set active GT.M version (aliased to "version")
#
#	Because setactive must be source'd, it cannot take any arguments (in csh).  We get around this by requiring the
#	shell variable setactive_parms to be set to a (possibly empty) list:
#
#	setactive_parms[1] - version
#			"A" or "a" => use (current) active version (i. e., don't change the version)
#			"D" or "d" => use current development (volatile) version
#			"P" or "p" => use current production (fully tested) version
#			omitted - same as "A" or "a"
#
#	setactive_parms[2] - type of binaries
#			"B" or "b" => use bta (beta) binaries (optimized with asserts but without debugger information)
#			"D" or "d" => use dbg (debug) binaries (unoptimized with asserts and debugger information)
#			"P" or "p" => use pro (production) binaries (optimized without asserts or debugger information)
#			omitted - use the current type of binary (or P if current type is not known)
#
#	If both arguments are omitted:
#			setactive will not change anything (no-op).
#
#######################################################################################################################


set setactive_switchto_ver = ""
set setactive_switchto_img = ""
if (! $?setactive_parms) then
	set setactive_parms = ("$1" "$2")
endif

if ( $#setactive_parms >= 1 ) then
	set setactive_switchto_ver = $setactive_parms[1]:au
endif

if ( $#setactive_parms >= 2 ) then
	set setactive_switchto_img = $setactive_parms[2]:au
endif

if (! $?gtm_root) then
	if ($?gtm_dist) then
		set gtm_root = $gtm_dist:h:h
	endif
endif

if (! $?gtm_root) then
	echo '$gtm_root is not defined and cannot be found using $gtm_dist'
	exit 1
endif

if ($?gtm_dist) then
	set setactive_current_ver = $gtm_dist:h:t
	set setactive_current_img = $gtm_dist:t
endif

# Determine the version to switch to
# If nothing is passed or if A is passed, stick to the current version
if ( ("" == "$setactive_switchto_ver") || ("A" == "$setactive_switchto_ver") ) then
	if ($?setactive_current_ver) then
		set setactive_switchto_ver = "$setactive_current_ver"
	else
		echo "SETACTIVE-E-CURRENT : $setactive_switchto_ver - Current Active version not known"
		exit 1
	endif
endif
# If "P" or "D" is passed, switch to the current production version defined in $gtm_curpro
if ( "$setactive_switchto_ver" =~ {P,D} ) then
	if ($?gtm_curpro) then
		set setactive_switchto_ver = "$gtm_curpro"
	else
		echo "SETACTIVE-E-CURPRO : $setactive_switchto_ver - Current Production version not defined in \$gtm_curpro"
		exit 1
	endif
endif

# Determine the image to switch to
# If nothing is passed, stick to the current image. If the current image is unknown switch to pro
if ( "" == "$setactive_switchto_img" ) then
	if ($?setactive_current_img) then
		set setactive_switchto_img = "$setactive_current_img"
	else
		set setactive_switchto_img = "pro"
	endif
endif
set setactive_switchto_img_type = "$setactive_switchto_img:au"
switch ($setactive_switchto_img_type)
	case "P*":
		set setactive_switchto_img = "pro"
		breaksw
	case "D*":
		set setactive_switchto_img = "dbg"
		breaksw
	case "B*":
		set setactive_switchto_img = "bta"
		breaksw
	default:
		echo "Image type $setactive_switchto_img_type is not known. Will switch to pro"
		set setactive_switchto_img = "pro"
		breaksw
endsw

# Now we know the current version & image and the to be switched to version & image
if (! -d $gtm_root/$setactive_switchto_ver) then
	echo "SETACTIVE-E-NOT_EXIST : $gtm_root/$setactive_switchto_ver does not exist"
	exit 1
endif

# Now we know the to be switched version exists. Setup all the environment variables

setenv gtm_ver		$gtm_root/$setactive_switchto_ver
setenv gtm_verno	$setactive_switchto_ver
setenv gtm_bta 		$gtm_ver/bta
setenv gtm_dbg		$gtm_ver/dbg
setenv gtm_pro		$gtm_ver/pro
setenv gtm_log		$gtm_ver/log
setenv gtm_exe		$gtm_ver/$setactive_switchto_img
setenv gtm_dist		$gtm_exe
setenv gtm_map		$gtm_exe/map
setenv gtm_obj		$gtm_exe/obj
setenv gtm_inc		$gtm_ver/inc
setenv gtm_pct		$gtm_ver/pct
setenv gtm_src		$gtm_ver/src
setenv gtm_tools	$gtm_ver/tools

# The below environment variable is set only for versions V63011 and earlier.
# This can be removed once there are no V63011 and prior versions.
setenv gtm_vrt		$gtm_ver
# unset environment variables set by prior versions. We will not set them going forward
unsetenv gtm_misc gtm_tags gtm_lint

# Replacing all $gtm_dist in gtmroutines to point to the new $gtm_dist is tricky.
# Simply removing all $gtm_dist now and adding them later changes the order and be unexpected.
# So, just reset gtmroutines. (Like how any changes to gt_cc* are simply lost when version is switched.)
setenv gtmroutines "."
source $gtm_tools/gtmsrc.csh

unset setactive_switchto_ver setactive_switchto_img setactive_switchto_img_type setactive_current_ver setactive_current_img
unset setactive_parms
