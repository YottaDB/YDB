#!/usr/local/bin/tcsh -f
#################################################################
#								#
# Copyright (c) 2001-2017 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
###########################################################################################
#
#	buildaux.csh - Build GT.M auxiliaries: dse, geteuid, gtmsecshr, lke, mupip.
#
#	Arguments:
#		$1 -	version number or code
#		$2 -	image type (b[ta], d[bg], or p[ro])
#		$3 -	target directory
#		$4 -	[auxillaries to build] e.g. dse mupip ftok gtcm_pkdisp gtcm_server etc.
#			Special value "shr" implies build "mumps" and ALL auxillaries
#
###########################################################################################

set buildaux_status = 0

source $gtm_tools/gtm_env.csh

if ( $1 == "" ) then
	@ buildaux_status++
endif

if ( $2 == "" ) then
	@ buildaux_status++
endif

if ( $3 == "" ) then
	@ buildaux_status++
endif


switch ($2)
case "[bB]*":
	set gt_ld_options = "$gt_ld_options_bta"
	set gt_image = "bta"
	breaksw

case "[dD]*":
	set gt_ld_options = "$gt_ld_options_dbg"
	set gt_image = "dbg"
	breaksw

case "[pP]*":
	set gt_ld_options = "$gt_ld_options_pro"
	set gt_image = "pro"
	breaksw

default:
	@ buildaux_status++
	breaksw

endsw


version $1 $2
if ( $buildaux_status ) then
	echo "buildaux-I-usage, Usage: buildaux.csh <version> <image type> <target directory> [auxillary]"
	exit $buildaux_status
endif

set buildaux_auxillaries = "gde dse geteuid gtmsecshr lke mupip gtcm_server gtcm_gnp_server gtmcrypt"
set buildaux_utilities = "semstat2 ftok gtcm_pkdisp gtcm_shmclean gtcm_play dbcertify"
set buildaux_executables = "$buildaux_auxillaries $buildaux_utilities"
set buildaux_validexecutable = 0

foreach executable ( $buildaux_executables )
	setenv buildaux_$executable 0
end

set new_auxillarylist = ""
set do_buildshr = 0
set skip_auxillaries = 0
if (4 <= $#) then
	if (($# == 4) && ("$4" == "shr")) then
		# "shr" is special value. Handle separately.
		# build "mumps" and ALL executables
		set do_buildshr = 1
		set argv[4] = ""
	endif
	foreach auxillary ( $argv[4-] )
		if ( "$auxillary" == "lke") then
			set new_auxillarylist = "$new_auxillarylist lke gtcm_gnp_server"
		else if ( "$auxillary" == "gnpclient") then
			set do_buildshr = 1
		else if ( "$auxillary" == "gnpserver") then
			set new_auxillarylist = "$new_auxillarylist gtcm_gnp_server"
		else if ( "$auxillary" == "cmisockettcp") then
			set do_buildshr = 1
			set new_auxillarylist = "$new_auxillarylist gtcm_gnp_server"
		else if ( "$auxillary" == "gtcm") then
			set new_auxillarylist = "$new_auxillarylist gtcm_server gtcm_play gtcm_shmclean gtcm_pkdisp"
		else if ( "$auxillary" == "stub") then
			set new_auxillarylist = "$new_auxillarylist dse mupip gtcm_server gtcm_gnp_server gtcm_play"
			set new_auxillarylist = "$new_auxillarylist gtcm_pkdisp gtcm_shmclean"
		else if ("$auxillary" == "mumps") then
			set do_buildshr = 1
			set skip_auxillaries = 1
		else
			set new_auxillarylist = "$new_auxillarylist $auxillary"
		endif
	end
endif

if ( $4 == "" ) then
	foreach executable ( $buildaux_executables )
		setenv buildaux_$executable 1
	end
else
	foreach executable ( $buildaux_executables )
		foreach auxillary ( $new_auxillarylist )
			if ( "$auxillary" == "$executable" ) then
				set buildaux_validexecutable = 1
				setenv buildaux_$auxillary 1
				break
			endif
		end
	end
	if ( $buildaux_validexecutable == 0 && "$new_auxillarylist" != "" ) then
		echo "buildaux-E-AuxUnknown -- Auxillary, ""$argv[4-]"", is not a valid one"
		echo "buildaux-I-usage, Usage: buildaux.csh <version> <image type> <target directory> [auxillary-list]"
		@ buildaux_status++
		exit $buildaux_status
	endif
endif

unalias ls rm cat

# The below 3 env vars are needed by buildaux_*.csh scripts
setenv dollar_sign \$
setenv mach_type `uname -m`
setenv platform_name `uname | sed 's/-//g' | tr '[A-Z]' '[a-z]'`

set cmdfile="$gtm_log/buildaux_$$"
rm -f $cmdfile $cmdfile.err
set outlist = ""
set dollar = '$'
set err_check = "if (${dollar}status) touch $cmdfile.err"
if ($do_buildshr) then
	set outfile = "${cmdfile}_buildshr.log"
	set redir=">& $outfile"
	set outlist = "$outlist $outfile"
	echo "($gtm_tools/buildshr.csh $1 $2 ${gtm_root}/$1/$2; $err_check) $redir &" >> $cmdfile.csh
endif

if (! $skip_auxillaries) then
	if ( $buildaux_gde == 1 ) then
		set outfile = "${cmdfile}_buildaux_gde.log"
		set redir=">& $outfile"
		set outlist = "$outlist $outfile"
		# Building GDE cannot happen parallely with buildshr as this stage requires "mumps" which is built by "buildshr".
		# Take that into account when parallelizing. If buildshr is also happening now, then defer buildgde to after that.
		if ($do_buildshr) then
			echo "wait" >> $cmdfile.csh
		endif
		echo "($gtm_tools/buildaux_gde.csh $gt_image; $err_check) $redir &" >> $cmdfile.csh
	endif
	set double_quote = '"'
	set args3 = "$gt_image ${double_quote}${gt_ld_options}${double_quote} $3"
	set args3exelist = "dse geteuid gtmsecshr lke mupip gtcm_server gtcm_gnp_server gtcm_play gtcm_pkdisp gtcm_shmclean"
	set args3exelist = "$args3exelist semstat2 ftok dbcertify"
	foreach exe ($args3exelist)
		set val = `eval echo '${'buildaux_${exe}'}'`
		if ($val == 1) then
			set outfile = "${cmdfile}_buildaux_${exe}.log"
			set redir=">& $outfile"
			set outlist = "$outlist $outfile"
			echo "($gtm_tools/buildaux_${exe}.csh $args3; $err_check) $redir &" >> $cmdfile.csh
		endif
	end
	# Create the plugin directory, copy the files and set it up so that build.sh can build the needed libraries.
	if ($buildaux_gtmcrypt == 1) then
		set outfile = "${cmdfile}_buildaux_gtmcrypt.log"
		set redir=">& $outfile"
		set outlist = "$outlist $outfile"
		echo "($gtm_tools/buildaux_gtmcrypt.csh; $err_check) $redir &" >> $cmdfile.csh
	endif
endif

echo "wait" >> $cmdfile.csh

set cmdout="$cmdfile.out"
source $cmdfile.csh >& $cmdout
set stat = $status

cat $outlist
rm $outlist

if ($stat) then
	cat $cmdout
	@ buildaux_status++
else
	rm $cmdfile.csh
	rm $cmdout
endif

if (-e $cmdfile.err) then
	cat $cmdfile.err
	rm -f $cmdfile.err
	@ buildaux_status++
endif

exit $buildaux_status
