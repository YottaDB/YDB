#################################################################
#								#
#	Copyright 2001, 2005 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
###############################################################################################
#
#	setactive1.csh - auxiliary script for setactive.csh
#			 (based on the VMS script GTM$COM:SETACTIVE.COM)
#
#	Because setactive.csh must set environment variables, it cannot be run as a subproces,
#	but rather must be source'd.  Unfortunately, the logic required is too complex (due,
#	apparently to problems buffering the command input stream), so setactive.csh invokes
#	this shell script as a subprocess to deal with the logic.  In order to pass information
#	back to the parent process, setactive1.csh writes a shell script to be source'd by the
#	parent process on completion.
#
#	Arguments:
#		$1 - new version (a, d, p, or specific version number)
#		$2 - image type (b, d, or p for bta, dbg, or pro)
#		$3 - 1 => interactive invocation, prompt for input and correcctions
#		     0 => non-interactive invocation
#		$4 - name of file to which to write setenv commands
#			(probably somewhere in /tmp)
#
#	At exit:
#		$status	== 0 => success
#			!= 0 => failure
#
#		If $status == 0, then the file named in $4 contains the necessary set and
#		setenv commands to effect a change of GT.M version.  This file should be
#		source'd by setactive.csh.
#
#	N. B. Most of the labels aren't actually used; they're just for cross-reference with
#	the VMS GTM$COM:SETACTIVE.COM file.
#
###############################################################################################

# Echo is modified for R-F test: Layek - 2/16/99
# Some times GROUP may be undefined
if ($?GROUP == 0) setenv GROUP ""

set setactive_status = 1	# not found yet

if ( $?gtm_ver_noecho == 0 ) then
	echo "Versions:"
	echo " Active      " $gtm_verno
	echo " Production  " $gtm_curpro
endif


set setactive_p1 = `echo $1 | tr '[a-z]' '[A-Z]'`
set setactive_p2 = "$2"
set setactive_interact = "$3"

set setactive_gtmdev = ( $gtm_gtmdev )

set setactive_prompt_for_type = "0"

unset setactive_found_version
unset setactive_doesnt_exist

GETVER:
while ( $?setactive_found_version == "0"  &&  $?setactive_doesnt_exist == "0" )

	if ( $setactive_interact == "1"  &&  $setactive_p1 == ""  &&  $setactive_p2 == "" ) then
		echo -n "Enter A, P, D, <version number> or <CR>: "
		set setactive_p1 = $<
		set setactive_prompt_for_type = "1"	# setactive_p1 won't be null anymore, but we can test for this
	endif

	switch ($setactive_p1)
	case "":
	case "[Aa]":
		echo "Version not changed"	# no change if unspecified or
		set setactive_found_version	# specified as currently active version
		breaksw

	case "[Dd]":
	case "[Pp]":
		setenv gtm_verno $gtm_curpro	# production version
		set setactive_found_version
		breaksw

	default:
		# It's not a code letter so assume it's a version name; check for existence of directory.

		# Convert first argument to its corresponding development directory name.
		set setactive_version = "`$shell -f $gtm_tools/gtm_version_dirname.csh $setactive_p1`"

VER_LOOP:
		# Now look for a matching release directory.
		# First look through all devices on which it might reside:
		foreach setactive_device ($gtm_gtmdev)

			if ( "$setactive_device" == "NULL" ) then
				set setactive_device = "~"
			endif
			set setactive_topdir = $setactive_device/$gtm_topdir

			if ( -d $setactive_topdir/$setactive_version ) then

			    	if ( -f $setactive_topdir/$setactive_version/gtmsrc.csh ) then
					setenv gtm_verno $setactive_version	# directory name, not input argument value
					set setactive_found_version

				else
					echo "$setactive_topdir/$setactive_version exists, but there's no gtmsrc.csh"
					set setactive_doesnt_exist		# actually, just isn't set up properly

				endif

				break
			endif
		end

		if ( $?setactive_found_version == "0"  ||  $?setactive_doesnt_exist == "1" ) then

			echo "Version $setactive_p1 is not available"
			set setactive_p1 = ""

			if ( $setactive_interact == "0" ) then
				echo "No action taken"
				set setactive_doesnt_exist
				set setactive_status = 2
				goto FINI
			else
				# Interactive invocation -- prompt for corrections.

				# First, list existing versions:
				echo ""
				echo "Available versions are:"
				echo ""

INDEX_LOOP:
				foreach setactive_device ($gtm_gtmdev)

DIR_LOOP:
					foreach setactive_verdir ($setactive_device/$gtm_topdir/V[0-9][0-9]*)

						if ( -d $setactive_verdir ) then
							set setactive_version = `basename $setactive_verdir`
							echo $setactive_version
						endif
					end
				end
			endif
		endif

		breaksw
	endsw
end

if ( $?setactive_found_version == "1" ) then
	set setactive_status = 0	# success

	if ( $?setactive_device == "0" ) then
LOOP_INDEX:
		# Since setactive_device is unset, we have not located the root directory for the release
		# named in gtm_verno (the version was probably specified by a code letter); look for it now.
		foreach setactive_device ($gtm_gtmdev)
			if ( "$setactive_device" == "NULL" ) then
				set setactive_device = "$gtm_gtmdev/"
			endif

			if ( -d $setactive_device/$gtm_topdir/$gtm_verno ) then
				break
			endif

		end
	endif

	# Note we set gtm_ver and gtm_vrt to the same value on Unix.  On VMS, these must be
	# separate logicals, one more-or-less normal and the other concealed.  For compatibility,
	# we should keep both environment variables distinct on Unix even though they have the
	# same value.
	setenv	gtm_ver	$setactive_device/$gtm_topdir/$gtm_verno
	setenv	gtm_vrt	$gtm_ver

	if ( -d $gtm_ver ) then
		if ( -f $gtm_ver/gtmsrc.csh ) then
			source $gtm_ver/gtmsrc.csh
			if ( $?gtm_ver_noecho == 0 ) echo "Version is now $gtm_verno"
		else
			echo "$gtm_ver exists, but $gtm_ver/gtmsrc.csh does not exist"
			set setactive_status = 3
		endif
	else
		echo "$gtm_ver does not exist"
		set setactive_status = 4
	endif
endif


set setactive_old_gtm_exe = `basename $gtm_exe`
switch ($setactive_old_gtm_exe)
case "[Bb][Tt][Aa]":
	set setactive_binary_desc = "optimized, with asserts, and without debugger information"
	breaksw

case "[Dd][Bb][Gg]":
	set setactive_binary_desc = "unoptimized, with asserts, and with debugger information"
	breaksw

case "[Pp][Rr][Oo]":
	set setactive_binary_desc = "optimized, without asserts, and without debugger information"
	breaksw

default:
	set setactive_binary_desc = "with unknown options"
	breaksw
endsw
if ( $?gtm_ver_noecho == 0 ) then
	echo ""
	echo "The previous binaries in use were from $gtm_exe"
	echo "which were compiled, assembled, and linked $setactive_binary_desc."
endif


unset setactive_found_type_code

# GETTYPE:
while ( $?setactive_found_type_code == "0" )

	if ( $setactive_prompt_for_type == "1" ) then
		echo -n "Enter b for beta (bta), d for debug (dbg), p production (pro), or <CR> for no change: "
		set setactive_p2 = $<
	endif

	if ( "$setactive_p2" == "" ) then
		setenv gtm_exe $gtm_vrt/$setactive_old_gtm_exe	# whatever it used to be
		set setactive_found_type_code		# a null at this point is intentional and means no change of binary type

	else
		switch ($setactive_p2)
		case "[Bb]*":
			setenv gtm_exe $gtm_ver/bta		# BTA
			set setactive_found_type_code
			breaksw

		case "[Dd]*":
			setenv gtm_exe $gtm_ver/dbg		# DBG
			set setactive_found_type_code
			breaksw

		case "[Pp]*":
			setenv gtm_exe $gtm_ver/pro		# PRO
			set setactive_found_type_code
			breaksw

		default:
			echo "Image type code $setactive_p2 is not known."
			if ( $setactive_interact == "1" ) then
				set setactive_p2 = ""		# throw away invalid value
			else
				setenv gtm_exe $gtm_vrt/$setactive_old_gtm_exe	# whatever it used to be
				set setactive_found_type_code	# can't correct it, just give up and take leave as is
				echo "Image types in use have not been changed."
			endif
			breaksw

		endsw
	endif
end

switch (`basename $gtm_exe`)
case "[Bb][Tt][Aa]":
	set setactive_binary_desc = "optimized, with asserts, and without debugger information"
	breaksw

case "[Dd][Bb][Gg]":
	set setactive_binary_desc = "unoptimized, with asserts, and with debugger information"
	breaksw

case "[Pp][Rr][Oo]":
	set setactive_binary_desc = "optimized, without asserts, and without debugger information"
	breaksw

default:
	set setactive_binary_desc = "with unknown options"
	breaksw
endsw
if ( $?gtm_ver_noecho == 0 ) then
	echo ""
	echo "The binaries in use are now from $gtm_exe"
	echo "which were compiled, assembled, and linked $setactive_binary_desc."
endif

echo "setenv gtm_verno	$gtm_verno"	>> $4
echo "setenv gtm_ver	$gtm_ver"	>> $4
echo "setenv gtm_vrt	$gtm_vrt"	>> $4
echo "setenv gtm_bta	$gtm_vrt/bta"	>> $4
echo "setenv gtm_dbg	$gtm_vrt/dbg"	>> $4
echo "setenv gtm_pro	$gtm_vrt/pro"	>> $4
if (-w $gtm_vrt/log ) then
	echo "setenv gtm_log    $gtm_vrt/log"   >> $4
else
	echo "SETACTIVE1-E-LOGPERM : $gtm_vrt/log does not have group write permissions. Please fix that."
        echo "setenv gtm_log    $gtm_log_path/$gtm_verno"   >> $4
endif
echo "setenv gtm_misc	$gtm_vrt/misc"	>> $4
echo "setenv gtm_tags	$gtm_vrt/misc/tags"	>> $4
echo "setenv gtm_exe	$gtm_exe"	>> $4
echo "setenv gtm_dist	$gtm_exe"	>> $4
echo "setenv gtm_lint	$gtm_exe/lint"	>> $4
echo "setenv gtm_map	$gtm_exe/map"	>> $4
echo "setenv gtm_obj	$gtm_exe/obj"	>> $4

FINI:

exit $setactive_status
