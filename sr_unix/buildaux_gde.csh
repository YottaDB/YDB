#!/usr/local/bin/tcsh -f
#################################################################
#								#
# Copyright (c) 2001-2016 Fidelity National Information		#
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
echo ""
echo "############# Linking GDE ###########"
echo ""
@ buildaux_gde_status = 0
source $gtm_tools/gtm_env.csh

set gt_image = $1

pushd $gtm_exe
chmod 664 *.m *.o

\rm -f *.m *.o	# use \rm to avoid rm from asking for confirmation (in case it has been aliased so)
cp -p $gtm_pct/*.m .
rm -f pinentry.m  # avoid problems with concurrent deletion
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
set echo
setenv LC_CTYPE C
setenv gtm_chset M
./mumps *.m
if ($status) then
	@ buildaux_gde_status++
	echo "buildaux-E-compile_M, Failed to compile .m programs in M mode" \
		>> $gtm_log/error.${gtm_exe:t}.log
endif
unset echo

source $gtm_tools/set_library_path.csh
source $gtm_tools/check_unicode_support.csh
if ("TRUE" == "$is_unicode_support") then
	if (! -e utf8) mkdir utf8
	if ( "OS/390" == $HOSTOS ) then
		setenv gtm_chset_locale $utflocale	# LC_CTYPE not picked up right
	endif
	set echo
	setenv LC_CTYPE $utflocale
	unsetenv LC_ALL
	setenv gtm_chset UTF-8	# switch to "UTF-8" mode
	unset echo
	\rm -f utf8/*.m	# use \rm to avoid rm from asking for confirmation (in case it has been aliased so)
	# get a list of all m files to link
	setenv mfiles `ls *.m`
	cd utf8
	foreach mfile ($mfiles)
		ln -s ../$mfile $mfile
	end
	set echo
	../mumps *.m
	if ($status) then
		@ buildaux_gde_status++
		echo "buildaux-E-compile_UTF8, Failed to compile .m programs in UTF-8 mode" \
			>> $gtm_log/error.${gtm_exe:t}.log
	endif
	unset echo
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
exit $buildaux_gde_status
