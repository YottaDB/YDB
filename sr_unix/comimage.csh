#!/usr/local/bin/tcsh
#################################################################
#								#
#	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

####################################################################
#
#	comimage.csh - submit background task to compile & link GT.M
#
#	comimage.csh is an interactive script to compile & link
#	(build) a release of GT.M.  It prompts the user for 4
#	parameters:
#		version	- version to be built (default: current)
#		images	- type of images (default: current)
#		C compiler options in addition to defaults
#		assembler options in addition to defaults
#
####################################################################
#

#	Get the version number/designation:
echo " "
echo -n "Enter Version		"
echo -n "[$gtm_verno]:	"
set comimage_ver = $<
if ( "$comimage_ver" == "" ) then
	set comimage_ver = $gtm_verno
endif
echo " "

#	Get the image type:
if ( "$gtm_exe" == "" ) then
	set comimage_image = "p"
else
	# Convert current image type to single-character prompt
	set comimage_image = `basename $gtm_exe`
	switch ( $comimage_image )
	case "b*":
			set comimage_image = "b"
			breaksw

	case "d*":
			set comimage_image = "d"
			breaksw

	case "p*":
	default:
			set comimage_image = "p"
			breaksw

	endsw
endif

echo -n "Enter Image		"
echo -n "[$comimage_image]:	"
set comimage_image_input = $<
if ( "$comimage_image_input" != "" ) then
	set comimage_image = $comimage_image_input
endif
echo " "

#	Convert to name and set default compiler and assembler options.
#	N.B.: These default options must be calculated the same way gtm_env.csh and gtm_env_sp.csh calculate them.
switch ( $comimage_image )
case "[Bb]*":
		set comimage_image = "bta"
		set comimage_as_options_default = \
			"$gt_as_options_common $gt_as_option_I $gt_as_option_DDEBUG $gt_as_option_optimize"
		set comimage_cc_options_default = \
			"$gt_cc_options_common $gt_cc_option_I $gt_cc_option_DDEBUG $gt_cc_option_optimize"
		breaksw

case "[Dd*]":
		set comimage_image = "dbg"
		set comimage_as_options_default = \
			"$gt_as_options_common $gt_as_option_I $gt_as_option_DDEBUG $gt_as_option_debug $gt_as_option_nooptimize"
		set comimage_cc_options_default = \
			"$gt_cc_options_common $gt_cc_option_I $gt_cc_option_DDEBUG $gt_cc_option_debug $gt_cc_option_nooptimize"
		breaksw

case "[Pp]*":
		set comimage_image = "pro"
		set comimage_as_options_default = \
			"$gt_as_options_common $gt_as_option_I $gt_as_option_optimize"
		set comimage_cc_options_default = \
			"$gt_cc_options_common $gt_cc_option_I $gt_cc_option_optimize"
		breaksw

endsw

#	Get assembler options:
echo "Enter additional assembler options"
echo "	[default: $comimage_as_options_default]"
echo -n '	-->	'
set comimage_as_options_extra = "$<"
if ( "$comimage_as_options_extra" != "" ) then
	echo "	[new: $comimage_as_options_default $comimage_as_options_extra]"
endif
echo " "

#	Get C compiler options:
echo "Enter additional C compiler options"
echo "	[default: $comimage_cc_options_default]"
echo -n '	-->	'
set comimage_cc_options_extra = "$<"
if ( "$comimage_cc_options_extra" != "" ) then
	echo "	[new: $comimage_cc_options_default $comimage_cc_options_extra]"
endif
echo " "


version $comimage_ver $comimage_image
if ( ! -d $gtm_ver/log ) then
	mkdir $gtm_ver/log
	chmod 775 $gtm_ver/log
endif
nohup /usr/local/bin/tcsh $gtm_tools/comlist.csh \
	"$comimage_as_options_extra" "$comimage_cc_options_extra" "gtm_$comimage_image" "$comimage_ver" \
	>& $gtm_ver/log/comlist.$comimage_image.log < /dev/null &

unset comimage_as_options_default
unset comimage_as_options_extra
unset comimage_cc_options_default
unset comimage_cc_options_extra
unset comimage_image
unset comimage_image_input
unset comimage_ver
