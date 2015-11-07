#################################################################
#								#
#	Copyright 2001, 2013 Fidelity Information Services, Inc #
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
#
###########################################################################################

set buildaux_status = 0

set dollar_sign = \$
set mach_type = `uname -m`
set platform_name = `uname | sed 's/-//g' | tr '[A-Z]' '[a-z]'`

if ( $1 == "" ) then
	set buildaux_status = `expr $buildaux_status + 1`
endif

if ( $2 == "" ) then
	set buildaux_status = `expr $buildaux_status + 1`
endif

if ( $3 == "" ) then
	set buildaux_status = `expr $buildaux_status + 1`
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
	set buildaux_status = `expr $buildaux_status + 1`
	breaksw

endsw


version $1 $2
if ( $buildaux_status != 0 ) then
	echo "buildaux-I-usage, Usage: $shell buildaux.csh <version> <image type> <target directory> [auxillary]"
	exit $buildaux_status
endif

#####################################################################################
#
# The executables comprises of auxillaries and utilities. Actually the later part
#	of the script can be done in a foreach loop. But for reasons of run-time
#	slow-down, we have to wait for hardware to improve on the slower platforms.
# This logical division is done so that later we can add the utilities in a loop.
#
#####################################################################################

set buildaux_auxillaries = "gde dse geteuid gtmsecshr lke mupip gtcm_server gtcm_gnp_server gtmcrypt"
set buildaux_utilities = "semstat2 ftok gtcm_pkdisp gtcm_shmclean gtcm_play dummy dbcertify"
set buildaux_executables = "$buildaux_auxillaries $buildaux_utilities"
set buildaux_validexecutable = 0

foreach executable ( $buildaux_executables )
	setenv buildaux_$executable 0
end

set new_auxillarylist = ""
if (4 <= $#) then
	foreach auxillary ( $argv[4-] )
		if ( "$auxillary" == "lke") then
			set new_auxillarylist = "$new_auxillarylist lke gtcm_gnp_server"
		else if ( "$auxillary" == "gnpclient") then
			$shell $gtm_tools/buildshr.csh $1 $2 ${gtm_root}/$1/$2
			if (0 != $status) @ buildaux_status = $status
		else if ( "$auxillary" == "gnpserver") then
			set new_auxillarylist = "$new_auxillarylist gtcm_gnp_server"
		else if ( "$auxillary" == "cmisockettcp") then
			set new_auxillarylist = "$new_auxillarylist gtcm_gnp_server"
			$shell $gtm_tools/buildshr.csh $1 $2 ${gtm_root}/$1/$2
		else if ( "$auxillary" == "gtcm") then
			set new_auxillarylist = "$new_auxillarylist gtcm_server gtcm_play gtcm_shmclean gtcm_pkdisp"
		else if ( "$auxillary" == "stub") then
			set new_auxillarylist = "$new_auxillarylist dse mupip gtcm_server gtcm_gnp_server gtcm_play"
			set new_auxillarylist = "$new_auxillarylist gtcm_pkdisp gtcm_shmclean"
		else if ("$auxillary" == "mumps") then
			$shell $gtm_tools/buildshr.csh $1 $2 ${gtm_root}/$1/$2
			if (0 != $status) @ buildaux_status = $status
			if ($#argv == 4) then
				exit $buildaux_status
			endif
		else if ( "$auxillary" == "gtmrpc" || "$auxillary" == "gtm_svc") then
			$shell $gtm_tools/buildshr.csh $1 $2 ${gtm_root}/$1/$2
			if (0 != $status) @ buildaux_status = $status
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
		echo "buildaux-I-usage, Usage: $shell buildaux.csh <version> <image type> <target directory> [auxillary-list]"
		set buildaux_status = `expr $buildaux_status + 1`
		exit $buildaux_status
	endif
endif

unalias ls rm
set buildaux_verbose = $?verbose
set verbose
set echo

if ( $buildaux_gde == 1 ) then
	pushd $gtm_exe
		chmod 664 *.m *.o

		\rm -f *.m *.o	# use \rm to avoid rm from asking for confirmation (in case it has been aliased so)
		cp $gtm_pct/*.m .
		switch ($gt_image)  # potentially all 3 versions could be in $gtm_pct .. we only need one, delete the others
		    case "pro":
			rm -f GTMDefinedTypesInitBta.m >& /dev/null
			rm -f GTMDefinedTypesInitDbg.m >& /dev/null
			mv GTMDefinedTypesInitPro.m GTMDefinedTypesInit.m
			breaksw
		    case "dbg":
			rm -f GTMDefinedTypesInitBta.m >& /dev/null
			rm -f GTMDefinedTypesInitPro.m >& /dev/null
			mv GTMDefinedTypesInitDbg.m GTMDefinedTypesInit.m
			breaksw
		    case "bta":
			rm -f GTMDefinedTypesInitDbg.m >& /dev/null
			rm -f GTMDefinedTypesInitPro.m >& /dev/null
			mv GTMDefinedTypesInitBta.m GTMDefinedTypesInit.m
			breaksw
		endsw
		# GDE and the % routines should all be in upper-case.
		if ( `uname` !~ "CYGWIN*") then
			ls -1 *.m | awk '! /GTMDefinedTypesInit/ {printf "mv %s %s\n", $1, toupper($1);}' | sed 's/.M$/.m/g' | sh
		else
			# unless the mount is "managed", Cygwin is case insensitive but preserving
			ls -1 *.m | awk '{printf "mv %s %s.tmp;mv %s.tmp %s\n", $1, $1, $1, toupper($1);}' | sed 's/.M$/.m/g' | sh
		endif

		# Compile all of the *.m files once so the $gtm_dist directory can remain protected.
		# Switch to M mode so we are guaranteed the .o files in this directory will be M-mode
		# 	(just in case current environment variables are in UTF8 mode)
		# Not doing so could cause later INVCHSET error if parent environment switches back to M mode.
		setenv LC_CTYPE C
		setenv gtm_chset M
		./mumps *.m
		if (0 != $status) @ buildaux_status = $status
		if ($buildaux_status != 0) then
			echo "buildaux-E-compile_M, Failed to compile .m programs in M mode" \
				>> $gtm_log/error.`basename $gtm_exe`.log
		endif

		source $gtm_tools/set_library_path.csh
		source $gtm_tools/check_unicode_support.csh
		if ("TRUE" == "$is_unicode_support") then
			if (! -e utf8) mkdir utf8
			if ( "OS/390" == $HOSTOS ) then
				setenv gtm_chset_locale $utflocale	# LC_CTYPE not picked up right
			endif
			setenv LC_CTYPE $utflocale
			unsetenv LC_ALL
			setenv gtm_chset UTF-8	# switch to "UTF-8" mode
			\rm -f utf8/*.m	# use \rm to avoid rm from asking for confirmation (in case it has been aliased so)
			# get a list of all m files to link
			setenv mfiles `ls *.m`
			cd utf8
			foreach mfile ($mfiles)
				ln -s ../$mfile $mfile
			end
			../mumps *.m
			if (0 != $status) @ buildaux_status = $status
			if ($buildaux_status != 0) then
				echo "buildaux-E-compile_UTF8, Failed to compile .m programs in UTF-8 mode" \
					>> $gtm_log/error.`basename $gtm_exe`.log
			endif
			cd ..
			setenv LC_CTYPE C
			unsetenv gtm_chset	# switch back to "M" mode
		endif

		# Don't deliver the GDE sources except with a dbg release.
		if ( "$gtm_exe" != "$gtm_dbg" ) then
			\rm -f GDE*.m	# use \rm to avoid rm from asking for confirmation (in case it has been aliased so)
			if (-e utf8) then
				\rm -f utf8/GDE*.m # use \rm to avoid rm from asking for confirmation (if it has been aliased so)
			endif
		endif
	popd
endif

if ( $buildaux_dse == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bcalls:$gtm_map/dse.loadmap -bmap:$gtm_map/dse.loadmap -bxref:$gtm_map/dse.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/dse	-L$gtm_obj $gtm_obj/{dse,dse_cmd}.o \
			$gt_ld_sysrtns $gt_ld_options_all_exe -ldse -lmumps -lstub \
			$gt_ld_extra_libs $gt_ld_syslibs >& $gtm_map/dse.map
	if ( $status != 0  ||  ! -x $3/dse ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkdse, Failed to link dse (see ${dollar_sign}gtm_map/dse.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
		if ( "dbg" == $gt_image ) then
			chatr +dbg enable +as mpas $3/dse
		else
			chatr +as mpas $3/dse
		endif
	endif
endif

if ( $buildaux_geteuid == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bcalls:$gtm_map/geteuid.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bmap:$gtm_map/geteuid.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bxref:$gtm_map/geteuid.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/geteuid	-L$gtm_obj $gtm_obj/geteuid.o \
			$gt_ld_sysrtns $gt_ld_extra_libs -lmumps $gt_ld_syslibs >& $gtm_map/geteuid.map
	if ( $status != 0  ||  ! -x $3/geteuid ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkgeteuid, Failed to link geteuid (see ${dollar_sign}gtm_map/geteuid.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
		if ( "dbg" == $gt_image ) then
			chatr +dbg enable $3/geteuid
		endif
	endif
endif

if ( $buildaux_gtmsecshr == 1 ) then
	set aix_loadmap_option = ''
	$gtm_com/IGS $3/gtmsecshr "STOP"	# stop any active gtmsecshr processes
	$gtm_com/IGS $3/gtmsecshr "RMDIR"	# remove root-owned gtmsecshr, gtmsecshrdir, gtmsecshrdir/gtmsecshr files/dirs
	foreach file (gtmsecshr gtmsecshr_wrapper)
		if ( $HOSTOS == "AIX") then
		    set aix_loadmap_option = "-bcalls:$gtm_map/$file.loadmap"
		    set aix_loadmap_option = "$aix_loadmap_option -bmap:$gtm_map/$file.loadmap"
		    set aix_loadmap_option = "$aix_loadmap_option -bxref:$gtm_map/$file.loadmap"
		endif
		gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/${file} -L$gtm_obj $gtm_obj/${file}.o \
				$gt_ld_sysrtns $gt_ld_extra_libs -lmumps $gt_ld_syslibs >& $gtm_map/${file}.map
		if ( $status != 0  ||  ! -x $3/${file} ) then
			set buildaux_status = `expr $buildaux_status + 1`
			echo "buildaux-E-link${file}, Failed to link ${file} (see ${dollar_sign}gtm_map/${file}.map)" \
				>> $gtm_log/error.`basename $gtm_exe`.log
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
	if (0 != $status) @ buildaux_status = $status
endif

if ( $buildaux_lke == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bcalls:$gtm_map/lke.loadmap -bmap:$gtm_map/lke.loadmap -bxref:$gtm_map/lke.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/lke	-L$gtm_obj $gtm_obj/{lke,lke_cmd}.o \
			$gt_ld_sysrtns $gt_ld_options_all_exe -llke -lmumps -lgnpclient -lmumps -lgnpclient -lcmisockettcp \
			$gt_ld_extra_libs $gt_ld_syslibs >& $gtm_map/lke.map
	if ( $status != 0  ||  ! -x $3/lke ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linklke, Failed to link lke (see ${dollar_sign}gtm_map/lke.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
		if ( "dbg" == $gt_image ) then
			chatr +dbg enable +as mpas $3/lke
		else
			chatr +as mpas $3/lke
		endif
	endif
endif

if ( $buildaux_mupip == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bcalls:$gtm_map/mupip.loadmap -bmap:$gtm_map/mupip.loadmap -bxref:$gtm_map/mupip.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/mupip	-L$gtm_obj $gtm_obj/{mupip,mupip_cmd}.o \
		$gt_ld_sysrtns $gt_ld_options_all_exe -lmupip -lmumps -lstub \
		$gt_ld_extra_libs $gt_ld_aio_syslib $gt_ld_syslibs >& $gtm_map/mupip.map
	if ( $status != 0  ||  ! -x $3/mupip ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkmupip, Failed to link mupip (see ${dollar_sign}gtm_map/mupip.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
		if ( "dbg" == $gt_image ) then
			chatr +dbg enable +as mpas $3/mupip
		else
			chatr +as mpas $3/mupip
		endif
	endif
endif

if ( $buildaux_gtcm_server == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bcalls:$gtm_map/gtcm_server.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bmap:$gtm_map/gtcm_server.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bxref:$gtm_map/gtcm_server.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/gtcm_server -L$gtm_obj \
		$gtm_obj/gtcm_main.o $gtm_obj/omi_srvc_xct.o $gt_ld_sysrtns $gt_ld_options_all_exe \
		-lgtcm -lmumps -lstub $gt_ld_extra_libs $gt_ld_syslibs >& $gtm_map/gtcm_server.map
	if ( $status != 0  ||  ! -x $3/gtcm_server) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkgtcm_server, Failed to link gtcm_server (see ${dollar_sign}gtm_map/gtcm_server.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
		if ( "dbg" == $gt_image ) then
			chatr +dbg enable +as mpas $3/gtcm_server
		else
			chatr +as mpas $3/gtcm_server
		endif
	endif
endif

if ( $buildaux_gtcm_gnp_server == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bcalls:$gtm_map/gtcm_gnp_server.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bmap:$gtm_map/gtcm_gnp_server.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bxref:$gtm_map/gtcm_gnp_server.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/gtcm_gnp_server -L$gtm_obj \
		$gtm_obj/gtcm_gnp_server.o $gt_ld_sysrtns $gt_ld_options_all_exe \
		-lgnpserver -llke -lmumps -lcmisockettcp -lstub \
		$gt_ld_extra_libs $gt_ld_syslibs >& $gtm_map/gtcm_gnp_server.map
	if ( $status != 0  ||  ! -x $3/gtcm_gnp_server) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkgtcm_gnp_server, Failed to link gtcm_gnp_server" \
			"(see ${dollar_sign}gtm_map/gtcm_gnp_server.map)" >> $gtm_log/error.`basename $gtm_exe`.log
	else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
		if ( "dbg" == $gt_image ) then
			chatr +dbg enable +as mpas $3/gtcm_gnp_server
		else
			chatr +as mpas $3/gtcm_gnp_server
		endif
	endif
endif


if ( $buildaux_gtcm_play == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bcalls:$gtm_map/gtcm_play.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bmap:$gtm_map/gtcm_play.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bxref:$gtm_map/gtcm_play.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/gtcm_play -L$gtm_obj \
		$gtm_obj/gtcm_play.o $gtm_obj/omi_sx_play.o $gt_ld_sysrtns $gt_ld_options_all_exe \
		-lgtcm -lmumps -lstub $gt_ld_extra_libs $gt_ld_syslibs >& $gtm_map/gtcm_play.map
	if ( $status != 0  ||  ! -x $3/gtcm_play) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkgtcm_play, Failed to link gtcm_play (see ${dollar_sign}gtm_map/gtcm_play.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
		if ( "dbg" == $gt_image ) then
			chatr +dbg enable +as mpas $3/gtcm_play
		else
			chatr +as mpas $3/gtcm_play
		endif
	endif
endif

if ( $buildaux_gtcm_pkdisp == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bcalls:$gtm_map/gtcm_pkdisp.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bmap:$gtm_map/gtcm_pkdisp.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bxref:$gtm_map/gtcm_pkdisp.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/gtcm_pkdisp -L$gtm_obj $gtm_obj/gtcm_pkdisp.o \
		$gt_ld_sysrtns -lgtcm -lmumps -lstub $gt_ld_extra_libs $gt_ld_syslibs \
			>& $gtm_map/gtcm_pkdisp.map
	if ( $status != 0  ||  ! -x $3/gtcm_pkdisp) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkgtcm_pkdisp, Failed to link gtcm_pkdisp (see ${dollar_sign}gtm_map/gtcm_pkdisp.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
		if ( "dbg" == $gt_image ) then
			chatr +dbg enable $3/gtcm_pkdisp
		endif
	endif
endif

if ( $buildaux_gtcm_shmclean == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bcalls:$gtm_map/gtcm_shmclean.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bmap:$gtm_map/gtcm_shmclean.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bxref:$gtm_map/gtcm_shmclean.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/gtcm_shmclean -L$gtm_obj $gtm_obj/gtcm_shmclean.o	\
		$gt_ld_sysrtns -lgtcm -lmumps -lstub $gt_ld_extra_libs $gt_ld_syslibs	\
			>& $gtm_map/gtcm_shmclean.map
	if ( $status != 0  ||  ! -x $3/gtcm_shmclean) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkgtcm_shmclean, Failed to link gtcm_shmclean (see ${dollar_sign}gtm_map/gtcm_shmclean.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
		if ( "dbg" == $gt_image ) then
			chatr +dbg enable $3/gtcm_shmclean
		endif
	endif
endif

if ( $buildaux_semstat2 == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bcalls:$gtm_map/semstat2.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bmap:$gtm_map/semstat2.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bxref:$gtm_map/semstat2.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/semstat2 -L$gtm_obj $gtm_obj/semstat2.o \
		$gt_ld_sysrtns $gt_ld_extra_libs $gt_ld_syslibs >& $gtm_map/semstat2.map
	if ( $status != 0  ||  ! -x $3/semstat2 ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linksemstat2, Failed to link semstat2 (see ${dollar_sign}gtm_map/semstat2.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
		if ( "dbg" == $gt_image ) then
			chatr +dbg enable $3/semstat2
		endif
	endif
endif

if ( $buildaux_ftok == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bcalls:$gtm_map/ftok.loadmap -bmap:$gtm_map/ftok.loadmap -bxref:$gtm_map/ftok.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/ftok -L$gtm_obj $gtm_obj/ftok.o \
			$gt_ld_sysrtns -lmumps $gt_ld_extra_libs $gt_ld_syslibs >& $gtm_map/ftok.map
	if ( $status != 0  ||  ! -x $3/ftok ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkftok, Failed to link ftok (see ${dollar_sign}gtm_map/ftok.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
		if ( "dbg" == $gt_image ) then
			chatr +dbg enable $3/ftok
		endif
	endif
endif

if ( $buildaux_dbcertify == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bcalls:$gtm_map/dbcertify.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bmap:$gtm_map/dbcertify.loadmap"
		set aix_loadmap_option = "$aix_loadmap_option -bxref:$gtm_map/dbcertify.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/dbcertify -L$gtm_obj \
		$gtm_obj/{dbcertify,dbcertify_cmd}.o $gt_ld_sysrtns -ldbcertify -lmupip -lmumps -lstub $gt_ld_aio_syslib \
		$gt_ld_extra_libs $gt_ld_syslibs >& $gtm_map/dbcertify.map
	if ( $status != 0  ||  ! -x $3/dbcertify ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkdbcertify, Failed to link dbcertify (see ${dollar_sign}gtm_map/dbcertify.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	else if ( "ia64" == $mach_type && "hpux" == $platform_name ) then
		if ( "dbg" == $gt_image ) then
			chatr +dbg enable +as mpas $3/dbcertify
		else
			chatr +as mpas $3/dbcertify
		endif
	endif
endif

# Create the plugin directory, copy the files and set it up so that build.sh can build the needed libraries.
if ($buildaux_gtmcrypt == 1) then
	set supported_list = `$gtm_tools/check_encrypt_support.sh mail`
	if ("FALSE" != "$supported_list") then		# Do it only on the platforms where encryption is supported
		set plugin_build_type=""
		switch ($2)
			case "[bB]*":
				set plugin_build_type="p"
				breaksw
			case "[pP]*":
				set plugin_build_type="p"
				breaksw
			default:
				set plugin_build_type="d"
				breaksw
		endsw
		rm -rf $gtm_dist/plugin/gtmcrypt
		rm -rf $gtm_dist/plugin/
		mkdir -p $gtm_dist/plugin/gtmcrypt >&! /dev/null
		setenv gtm_dist_plugin $gtm_dist/plugin/gtmcrypt
		cp -pf $gtm_src/gtmcrypt_*ref.c $gtm_src/maskpass.c $gtm_dist_plugin >&! /dev/null
		cp -pf $gtm_inc/gtmcrypt_*ref.h $gtm_inc/gtmcrypt_interface.h $gtm_dist_plugin >&! /dev/null
		cp -pf $gtm_tools/build.sh $gtm_tools/install.sh $gtm_tools/add_db_key.sh $gtm_dist_plugin >&! /dev/null
		cp -pf $gtm_tools/encrypt_sign_db_key.sh $gtm_tools/gen_keypair.sh $gtm_dist_plugin >&! /dev/null
		cp -pf $gtm_tools/gen_sym_hash.sh $gtm_tools/gen_sym_key.sh $gtm_dist_plugin >&! /dev/null
		cp -pf $gtm_tools/import_and_sign_key.sh $gtm_tools/pinentry-gtm.sh $gtm_dist_plugin >&! /dev/null
		cp -pf $gtm_tools/show_install_config.sh $gtm_dist_plugin >&! /dev/null
		cp -pf $gtm_pct/pinentry.m $gtm_dist_plugin >&! /dev/null
		chmod +x $gtm_dist_plugin/*.sh
		pushd $gtm_dist_plugin >&! /dev/null
		# For, non-AIX platforms, move the libgcrypt entry to the end of $supported_list. This way, build.sh will always
		# build maskpass with libgcrypt dependency rather than libcrypto dependency on non-AIX platforms.
		if ($HOSTOS != "AIX") then
			set supported_list_reorder = `echo $supported_list | sed 's/gcrypt//;s/$/ gcrypt/'`
			set supported_list = `echo $supported_list_reorder`
		endif
		# Build all possible encryption libraries based on what encryption libraries are supported in this platform.
		foreach supported_lib ($supported_list)
			foreach algorithm ("AES256CFB" "BLOWFISHCFB")
				if ("gcrypt" == "$supported_lib" && "BLOWFISHCFB" == "$algorithm") continue
				echo "####### Building encryption plugin using $supported_lib with $algorithm algorithm #########"
				sh -x $gtm_dist_plugin/build.sh $supported_lib $plugin_build_type $algorithm
				set bstat = $status
				echo ""
				if (0 != $bstat) then
					set buildaux_status = `expr $buildaux_status + 1`
					echo "buildaux-E-libgtmcrypt, failed to build gtmcrypt and/or helper scripts. See 'map'	\
						files in ${dollar_sign}gtm_dist/plugin/gtmcrypt for more details" 		\
									>> $gtm_log/error.`basename $gtm_exe`.log
				endif
			end
		end
		if ($gtm_verno =~ V[4-8]*) then
			# For production builds don't do any randomizations.
			set algorithm = "AES256CFB"
			if ($HOSTOS == "AIX") then
				set encryption_lib = "openssl"
			else
				set encryption_lib = "gcrypt"
			endif
		else
			# Now that we've built "possibly" more than one encryption library, choose one configuration (based on
			# third-party library and algorithm) randomly and install that.
			set rand = `$gtm_dist/mumps -run %XCMD 'write 1+$random('$#supported_list')'`
			set encryption_lib = $supported_list[$rand]
			if ("gcrypt" == "$encryption_lib") then
				# Force AES as long as the plugin is linked against libgcrypt
				set algorithm = "AES256CFB"
			else
				# OpenSSL, V9* build. Go ahead and randomize the algorithm
				# increase probability of AES256CFB, the industry standard and the one we officially support
				set algorithms = ("AES256CFB" "AES256CFB" "BLOWFISHCFB")
				set rand = `$gtm_dist/mumps -run %XCMD 'write 1+$random('$#algorithms')'`
				set algorithm = $algorithms[$rand]
			endif
		endif
		$gtm_dist_plugin/install.sh $encryption_lib $algorithm
		if (0 != $status) then
			set buildaux_status = `expr $buildaux_status + 1`
			echo "buildaux-E-libgtmcrypt, failed to install libgtmcrypt and/or helper scripts"		\
						>> $gtm_log/error.`basename $gtm_exe`.log
		endif
		popd >&! /dev/null
	endif
endif

unset buildaux_m

unset echo
if ( $buildaux_verbose == "0" ) then
	unset verbose
endif

exit $buildaux_status
