#!/usr/local/bin/tcsh
#################################################################
#								#
#	Copyright 2001 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

##################################################################
#
#	lintimage.csh - submit background task to lint GT.M
#
#	lintimage.csh is an interactive script to run lint on a
#	release of GT.M.  It prompts the user for 3
#	parameters:
#		version	- version to be built (default: current)
#		images	- type of images (default: current)
#		lint options in addition to defaults
#
##################################################################
#

#	Get the version number/designation:
echo " "
echo -n "Enter Version		"
echo -n "[$gtm_verno]:	"
set lintimage_ver = $<
if ( "$lintimage_ver" == "" ) then
	set lintimage_ver = $gtm_verno
endif
echo " "

#	Get the image type:
if ( "$gtm_exe" == "" ) then
	set lintimage_image = "p"
else
	# Convert current image type to single-character prompt
	set lintimage_image = `basename $gtm_exe`
	switch ( $lintimage_image )
	case "b*":
			set lintimage_image = "b"
			breaksw

	case "d*":
			set lintimage_image = "d"
			breaksw

	case "p*":
	default:
			set lintimage_image = "p"
			breaksw

	endsw
endif

echo -n "Enter Image		"
echo -n "[$lintimage_image]:	"
set lintimage_image_input = $<
if ( "$lintimage_image_input" != "" ) then
	set lintimage_image = $lintimage_image_input
endif
echo " "

#	Convert to name and set default compiler and assembler options.
#	N.B.: These default options must be calculated the same way gtm_env.csh and gtm_env_sp.csh calculate them.
switch ( $lintimage_image )
case "[Bb]*":
		set lintimage_image = "bta"
		set lintimage_lint_options_default = "$gt_lint_options_common $gt_lint_options_bta"
		breaksw

case "[Dd*]":
		set lintimage_image = "dbg"
		set lintimage_lint_options_default = "$gt_lint_options_common $gt_lint_options_dbg"
		breaksw

case "[Pp]*":
		set lintimage_image = "pro"
		set lintimage_lint_options_default = "$gt_lint_options_common $gt_lint_options_pro"
		breaksw

endsw

#	Get lint options:
echo "Enter additional lint options"
echo "	[default: $lintimage_lint_options_default]"
echo -n '	-->	'
set lintimage_lint_options_extra = "$<"
if ( "$lintimage_lint_options_extra" != "" ) then
	echo "	[new: $lintimage_lint_options_default $lintimage_lint_options_extra]"
endif
echo " "


version $lintimage_ver $lintimage_image
if ( ! -d $gtm_ver/log ) then
	mkdir $gtm_ver/log
	chmod 775 $gtm_ver/log
endif
nohup /usr/local/bin/tcsh $gtm_tools/lintgtm.csh \
	"$lintimage_ver" "gtm_$lintimage_image" "$lintimage_lint_options_extra" \
	>& $gtm_ver/log/lintgtm.$lintimage_image.log < /dev/null &

unset lintimage_lint_options_default
unset lintimage_lint_options_extra
unset lintimage_image
unset lintimage_image_input
unset lintimage_ver
