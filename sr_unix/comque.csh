#!/usr/local/bin/tcsh
#################################################################
#								#
#	Copyright 2001, 2012 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

##################################################################
#
#	comque.csh - submit background task to compile & link GT.M
#
#	comque.csh is an interactive script to compile & link
#	(build) a release of GT.M.  It prompts the user for 4
#	parameters:
#		version	- version to be built (default: current)
#		images	- type of images (default: current)
#		C compiler options in addition to defaults
#		assembler options in addition to defaults
#
##################################################################
#

#	Do not build as root. If you build as root, you'll endu up with incorrectly set permissions on some files.
#	Incorrectly set permissions fail kitstart.csh

if ( 0 == `id -u` ) then
    echo "You can not build as root."
    exit 1
endif
#	Get rid of debug options producing huge amounts of output if set
unsetenv gtmdbglvl
#	Get the version number/designation:
echo " "
echo -n "Enter Version		"
echo -n "[$gtm_verno]:	"
if ( $?comque_batch_mode != 0) then
	set comque_ver = ""
else
	set comque_ver = $<
endif
if ( "$comque_ver" == "" ) then
	set comque_ver = $gtm_verno
endif
echo " "

#	Get the image type:
if ( "$gtm_exe" == "" ) then
	set comque_image = "p"
else
	# Convert current image type to single-character prompt
	set comque_image = `basename $gtm_exe`
	switch ( $comque_image )
	case "b*":
			set comque_image = "b"
			breaksw

	case "d*":
			set comque_image = "d"
			breaksw

	case "p*":
	default:
			set comque_image = "p"
			breaksw

	endsw
endif

echo -n "Enter Image		"
echo -n "[$comque_image]:	"
if ( $?comque_batch_mode != 0) then
	set comque_image_input = ""
else
	set comque_image_input = $<
endif
if ( "$comque_image_input" != "" ) then
	set comque_image = $comque_image_input
endif
echo " "

#	Convert to name and set default compiler and assembler options.
#	N.B.: These default options must be calculated the same way gtm_env.csh and gtm_env_sp.csh calculate them.
switch ( $comque_image )
case "[Bb]*":
		set comque_image = "bta"
		set comque_as_options_default = \
			"$gt_as_options_common $gt_as_option_I $gt_as_option_DDEBUG $gt_as_option_optimize"
		set comque_cc_options_default = \
			"$gt_cc_options_common $gt_cc_option_I $gt_cc_option_DDEBUG $gt_cc_option_optimize"
		breaksw

case "[Dd*]":
		set comque_image = "dbg"
		set comque_as_options_default = \
			"$gt_as_options_common $gt_as_option_I $gt_as_option_DDEBUG $gt_as_option_debug $gt_as_option_nooptimize"
		set comque_cc_options_default = \
			"$gt_cc_options_common $gt_cc_option_I $gt_cc_option_DDEBUG $gt_cc_option_debug $gt_cc_option_nooptimize"
		breaksw

case "[Pp]*":
		set comque_image = "pro"
		set comque_as_options_default = \
			"$gt_as_options_common $gt_as_option_I $gt_as_option_optimize"
		set comque_cc_options_default = \
			"$gt_cc_options_common $gt_cc_option_I $gt_cc_option_optimize"
		breaksw

endsw

#	Get assembler options:
echo "Enter additional assembler options"
echo "	[default: $comque_as_options_default]"
echo -n '	-->	'
if ( $?comque_batch_mode != 0) then
	set comque_as_options_extra = ""
else
	set comque_as_options_extra = "$<"
endif
if ( "$comque_as_options_extra" != "" ) then
	echo "	[new: $comque_as_options_default $comque_as_options_extra]"
endif
echo " "

#	Get C compiler options:
echo "Enter additional C compiler options"
echo "	[default: $comque_cc_options_default]"
echo -n '	-->	'
if ( $?comque_batch_mode != 0) then
	set comque_cc_options_extra = ""
else
	set comque_cc_options_extra = "$<"
endif
if ( "$comque_cc_options_extra" != "" ) then
	echo "	[new: $comque_cc_options_default $comque_cc_options_extra]"
endif
echo " "


version $comque_ver $comque_image
if ( ! -d $gtm_ver/log ) then
	mkdir $gtm_ver/log
	chmod 775 $gtm_ver/log
endif

if ( $?comque_no_background != 0) then
	if ( $?comque_batch_mode != 0) then
		/usr/local/bin/tcsh $gtm_tools/comlist.csh \
			"$comque_as_options_extra" "$comque_cc_options_extra" "gtm_$comque_image" "$comque_ver" < /dev/null \
				>& $gtm_ver/log/comlist.$comque_image.log
	else
		/usr/local/bin/tcsh $gtm_tools/comlist.csh \
			"$comque_as_options_extra" "$comque_cc_options_extra" "gtm_$comque_image" "$comque_ver" < /dev/null \
				|& tee $gtm_ver/log/comlist.$comque_image.log
	endif
else
	nohup /usr/local/bin/tcsh $gtm_tools/comlist.csh \
		"$comque_as_options_extra" "$comque_cc_options_extra" "gtm_$comque_image" "$comque_ver" \
		>& $gtm_ver/log/comlist.$comque_image.log < /dev/null &
endif

unset comque_as_options_default
unset comque_as_options_extra
unset comque_cc_options_default
unset comque_cc_options_extra
unset comque_image
unset comque_image_input
unset comque_ver
