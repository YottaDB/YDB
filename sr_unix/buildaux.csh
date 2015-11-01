#################################################################
#								#
#	Copyright 2001, 2006 Fidelity Information Services, Inc #
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
#		$4 -    [auxillaries to build] e.g. dse mupip ftok gtcm_pkdisp gtcm_server etc.
#
###########################################################################################

set buildaux_status = 0

set dollar_sign = \$

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
	breaksw

case "[dD]*":
	set gt_ld_options = "$gt_ld_options_dbg"
	breaksw

case "[pP]*":
	set gt_ld_options = "$gt_ld_options_pro"
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

set buildaux_auxillaries = "gde dse geteuid  gtmsecshr lke mupip gtcm_server gtcm_gnp_server"
set buildaux_utilities = "semstat2 ftok gtcm_pkdisp gtcm_shmclean gtcm_play dummy dbcertify"
set buildaux_executables = "$buildaux_auxillaries $buildaux_utilities"
set buildaux_validexecutable = 0

foreach executable ( $buildaux_executables )
	setenv buildaux_$executable 0
end

set new_auxillarylist = ""
foreach auxillary ( $argv[4-] )
	if ( "$auxillary" == "lke") then
		set new_auxillarylist = "$new_auxillarylist lke gtcm_gnp_server"
	else if ( "$auxillary" == "gnpclient") then
		$shell $gtm_tools/buildshr.csh $1 $2 ${gtm_root}/$1/$2
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
		if ($#argv == 4) then
			exit $buildaux_status
		endif
	else if ( "$auxillary" == "gtmrpc" || "$auxillary" == "gtm_svc") then
		$shell $gtm_tools/buildshr.csh $1 $2 ${gtm_root}/$1/$2
	else
		set new_auxillarylist = "$new_auxillarylist $auxillary"
	endif
end

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

unalias ls
set buildaux_verbose = $?verbose
set verbose
set echo

if ( $buildaux_gde == 1 ) then
	pushd $gtm_exe
		chmod 664 *.m *.o
		rm *.m *.o
		cp $gtm_pct/*.m .

		# GDE and the % routines should all be in upper-case.
		ls -1 *.m | awk '{printf "mv %s %s\n", $1, toupper($1);}' | sed 's/.M$/.m/g' | sh

		# Compile all of the *.m files once so the $gtm_dist directory can remain protected.
		mumps *.m
		if ($status != 0) then
			set buildaux_status = `expr $buildaux_status + 1`
			echo "buildaux-E-compile_M, Failed to compile .m programs in M mode" \
				>> $gtm_log/error.`basename $gtm_exe`.log
		endif

		if (`uname` == "AIX") then
			setenv LIBPATH /usr/local/lib
			set library_path = "$LIBPATH"
		else
			setenv LD_LIBRARY_PATH "/usr/local/lib:/usr/lib"
			set library_path = `echo $LD_LIBRARY_PATH | sed 's/:/ /g'`
		endif
		set utflocale = `locale -a | grep -i en_us | grep -i utf | grep '8$'`
		if (! -e utf8) then
			foreach libpath ($library_path)
				set found_icu = (0 != `\ls -1 $libpath/* | grep "libicu.*36.*" | wc -l`)
				if ($found_icu) then
					break
				endif
			end
			if ($found_icu && $utflocale != "") then
				mkdir utf8
			endif
		endif
		if (-e utf8) then
			setenv LC_CTYPE $utflocale
			unsetenv LC_ALL
			setenv gtm_chset UTF-8	# switch to "UTF-8" mode
			rm -f utf8/*.m
			cp *.m utf8
			cd utf8
			../mumps *.m
			if ($status != 0) then
				set buildaux_status = `expr $buildaux_status + 1`
				echo "buildaux-E-compile_UTF8, Failed to compile .m programs in UTF-8 mode" \
					>> $gtm_log/error.`basename $gtm_exe`.log
			endif
			cd ..
			setenv LC_CTYPE C
			unsetenv gtm_chset	# switch back to "M" mode
		endif

		# Don't deliver the GDE sources except with a dbg release.
		if ( "$gtm_exe" != "$gtm_dbg" ) then
			rm GDE*.m
			if (-e utf8) then
				rm utf8/GDE*.m
			endif
		endif
	popd
endif

if ( $buildaux_dse == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/dse.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/dse	-L$gtm_obj $gtm_obj/{dse,dse_cmd}.o \
			$gt_ld_sysrtns -ldse -lmumps -lstub \
			$gt_ld_syslibs >& $gtm_map/dse.map
	if ( $status != 0  ||  ! -x $3/dse ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkdse, Failed to link dse (see ${dollar_sign}gtm_map/dse.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif

if ( $buildaux_geteuid == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/geteuid.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/geteuid	-L$gtm_obj $gtm_obj/geteuid.o \
			$gt_ld_sysrtns -lmumps $gt_ld_syslibs >& $gtm_map/geteuid.map
	if ( $status != 0  ||  ! -x $3/geteuid ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkgeteuid, Failed to link geteuid (see ${dollar_sign}gtm_map/geteuid.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif

if ( $buildaux_gtmsecshr == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/gtmsecshr.loadmap"
	endif
	$gtm_com/IGS $3/gtmsecshr 1
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/gtmsecshr	-L$gtm_obj $gtm_obj/gtmsecshr.o \
			$gt_ld_sysrtns -lmumps $gt_ld_syslibs >& $gtm_map/gtmsecshr.map
	if ( $status != 0  ||  ! -x $3/gtmsecshr ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkgtmsecshr, Failed to link gtmsecshr (see ${dollar_sign}gtm_map/gtmsecshr.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
	$gtm_com/IGS $3/gtmsecshr 2
endif

if ( $buildaux_lke == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/lke.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/lke	-L$gtm_obj $gtm_obj/{lke,lke_cmd}.o \
			$gt_ld_sysrtns -llke -lmumps -lgnpclient -lmumps -lgnpclient -lcmisockettcp \
			$gt_ld_syslibs >& $gtm_map/lke.map
	if ( $status != 0  ||  ! -x $3/lke ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linklke, Failed to link lke (see ${dollar_sign}gtm_map/lke.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif

if ( $buildaux_mupip == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/mupip.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/mupip	-L$gtm_obj $gtm_obj/{mupip,mupip_cmd}.o \
		$gt_ld_sysrtns -lmupip -lmumps -lstub $gt_ld_aio_syslib $gt_ld_syslibs >& $gtm_map/mupip.map
	if ( $status != 0  ||  ! -x $3/mupip ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkmupip, Failed to link mupip (see ${dollar_sign}gtm_map/mupip.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif

if ( $buildaux_gtcm_server == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/gtcm_server.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/gtcm_server -L$gtm_obj \
		$gtm_obj/gtcm_main.o $gtm_obj/omi_srvc_xct.o $gt_ld_sysrtns \
		-lgtcm -lmumps -lstub $gt_ld_syslibs >& $gtm_map/gtcm_server.map
	if ( $status != 0  ||  ! -x $3/gtcm_server) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkgtcm_server, Failed to link gtcm_server (see ${dollar_sign}gtm_map/gtcm_server.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif

if ( $buildaux_gtcm_gnp_server == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/gtcm_gnp_server.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/gtcm_gnp_server -L$gtm_obj \
		$gtm_obj/gtcm_gnp_server.o $gtm_obj/gtcm_gnp_clitab.o $gt_ld_sysrtns \
		-lgnpserver -llke -lmumps -lcmisockettcp -lstub \
		$gt_ld_syslibs >& $gtm_map/gtcm_gnp_server.map
	if ( $status != 0  ||  ! -x $3/gtcm_gnp_server) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkgtcm_gnp_server, Failed to link gtcm_gnp_server" \
			"(see ${dollar_sign}gtm_map/gtcm_gnp_server.map)" >> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif


if ( $buildaux_gtcm_play == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/gtcm_play.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/gtcm_play -L$gtm_obj \
		$gtm_obj/gtcm_play.o $gtm_obj/omi_sx_play.o $gt_ld_sysrtns \
		-lgtcm -lmumps -lstub $gt_ld_syslibs >& $gtm_map/gtcm_play.map
	if ( $status != 0  ||  ! -x $3/gtcm_play) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkgtcm_play, Failed to link gtcm_play (see ${dollar_sign}gtm_map/gtcm_play.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif

if ( $buildaux_gtcm_pkdisp == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/gtcm_pkdisp.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/gtcm_pkdisp -L$gtm_obj $gtm_obj/gtcm_pkdisp.o \
		$gt_ld_sysrtns -lgtcm -lmumps -lstub $gt_ld_syslibs \
			>& $gtm_map/gtcm_pkdisp.map
	if ( $status != 0  ||  ! -x $3/gtcm_pkdisp) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkgtcm_pkdisp, Failed to link gtcm_pkdisp (see ${dollar_sign}gtm_map/gtcm_pkdisp.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif

if ( $buildaux_gtcm_shmclean == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/gtcm_shmclean.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/gtcm_shmclean -L$gtm_obj $gtm_obj/gtcm_shmclean.o	\
		$gt_ld_sysrtns -lgtcm -lmumps -lstub $gt_ld_syslibs			\
			>& $gtm_map/gtcm_shmclean.map
	if ( $status != 0  ||  ! -x $3/gtcm_shmclean) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkgtcm_shmclean, Failed to link gtcm_shmclean (see ${dollar_sign}gtm_map/gtcm_shmclean.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif

if ( $buildaux_semstat2 == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/semstat2.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/semstat2 -L$gtm_obj $gtm_obj/semstat2.o \
		$gt_ld_sysrtns $gt_ld_syslibs >& $gtm_map/semstat2.map
	if ( $status != 0  ||  ! -x $3/semstat2 ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linksemstat2, Failed to link semstat2 (see ${dollar_sign}gtm_map/semstat2.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif

if ( $buildaux_ftok == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/ftok.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/ftok -L$gtm_obj $gtm_obj/ftok.o \
			$gt_ld_sysrtns $gt_ld_syslibs >& $gtm_map/ftok.map
	if ( $status != 0  ||  ! -x $3/ftok ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkftok, Failed to link ftok (see ${dollar_sign}gtm_map/ftok.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif

if ( $buildaux_dbcertify == 1 ) then
	set aix_loadmap_option = ''
	if ( $HOSTOS == "AIX") then
		set aix_loadmap_option = "-bloadmap:$gtm_map/dbcertify.loadmap"
	endif
	gt_ld $gt_ld_options $aix_loadmap_option ${gt_ld_option_output}$3/dbcertify -L$gtm_obj \
		$gtm_obj/{dbcertify,dbcertify_cmd}.o $gt_ld_sysrtns -ldbcertify -lmupip -lmumps -lstub $gt_ld_aio_syslib \
		$gt_ld_syslibs >& $gtm_map/dbcertify.map
	if ( $status != 0  ||  ! -x $3/dbcertify ) then
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-linkdbcertify, Failed to link dbcertify (see ${dollar_sign}gtm_map/dbcertify.map)" \
			>> $gtm_log/error.`basename $gtm_exe`.log
	endif
endif

if ( $buildaux_dummy == 1 ) then
endif

unset buildaux_m

unset echo
if ( $buildaux_verbose == "0" ) then
	unset verbose
endif

exit $buildaux_status
