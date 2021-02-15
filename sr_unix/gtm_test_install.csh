#!/usr/local/bin/tcsh -f
#################################################################
#								#
# Copyright (c) 2011-2020 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
echo ""
echo "------------------------------------------------------------------------"
set arch = `grep arch $gtm_ver/pro/arch.gtc | awk -F= '{print $2}' | tr -d '\"'`
cd $1
# Source the installed GT.M's configuration
source ./gtmcshrc
setenv gtmgbldir mumps.gld
gde exit
mupip create
gtm << \EOF						>&! gtm_test_install.out
set ^X=1
set x=2
write $ORDER(^%),!
zwrite
zwrite ^X
halt
\EOF
gtm << EOF >>&! gtm_test_install.out
zhelp

zwrite ^X
halt
EOF


mkdir test_gtm
cd test_gtm

# create sim.m
cat > sim.m <<SIM
sim
	set ^a=1
	set ^b=2
	write "^a= ",^a," ^b= ",^b,!
	write "ZCHSET= ",\$zchset,!
	halt
SIM

setenv save_gtm_dist $gtm_dist
# unset gtm_dist for gtm to generate mumps.gld and journal file
unsetenv gtm_dist
# set to test directory
setenv gtmdir $save_gtm_dist/test_gtm
# NOTE: this is not the alias gtm, but the script sr_unix/gtm.gtc
../gtm -r sim >& gtm.out
# get $ZCHSET
echo ""							>>&! $save_gtm_dist/gtm_test_install.out
grep ZCHSET gtm.out					>>&! $save_gtm_dist/gtm_test_install.out

#get version number

setenv gtmver `ls -d V*`

# get journal output
cd $gtmver/g

setenv gtm_dist $save_gtm_dist

$gtm_dist/mupip journal -extract -forward gtm.mjl 				>& mupip.out
env gtmgbldir=$gtm_dist/test_gtm/mumps.gld $gtm_dist/mupip integ -reg "*"	>& integ.out
set mupip_status = $status
# output the change lines
echo ""							>>&! $gtm_dist/gtm_test_install.out
echo "Global changes in the gtm.mjl file:"		>>&! $gtm_dist/gtm_test_install.out
grep = gtm.mjf | awk -F\\ '{print ($NF)}'		>>&! $gtm_dist/gtm_test_install.out
if (0 != $mupip_status) then
	echo "Error: mupip integ returned $status status instead of 0" >>&! $gtm_dist/gtm_test_install.out
else
	echo "mupip integ returned a 0 status - as expected" >>&! $gtm_dist/gtm_test_install.out
endif

cd $gtm_dist

# If gtm_icu_version is empty, consider it equivalent to not set
if ($?gtm_icu_version) then
	if ("" != $gtm_icu_version) then
		unsetenv gtm_icu_version
	endif
endif
set icu_version = ""
set libpath = ""
if (! $?gtm_icu_version) then
	# icu-config is deprecated. So try "pkg-config icu-io" first, followed by "icu-config" and "pkg-config icu"
	if ( (-X pkg-config) && ( { pkg-config --exists icu-io } ) ) then
		set versioncmd = "pkg-config --modversion icu-io"
		set libcmd="pkg-config --variable=libdir icu-io"
	else if (-X icu-config) then
		set versioncmd = "icu-config --version"
		set libcmd="icu-config --libdir"
	else if ( (-X pkg-config) && ( { pkg-config --exists icu } ) ) then
		set versioncmd = "pkg-config --modversion icu"
		set libcmd="pkg-config --variable=libdir icu"
	endif
	if ($?versioncmd) then
		# pkg-config/icu-config can report versions like 4.2.1 but we want just the 4.2 part for GT.M
		set icu_version = `$versioncmd | awk '{ver=+$0;if(ver>5){ver=ver/10}printf("%.1f\n",ver);exit}'`
		set libpath=`$libcmd`
		if ( "linux" == "$arch" ) then
			# We do not recommend setting gtm_icu_version on AIX
			setenv gtm_icu_version "$icu_version"
		endif
	endif
endif

# No files are supposed to have write permissions enabled. However part of the kit install
# test leaves a few files writeable. The list of known writeable files is:
# 	.:
# 		-rw-r--r-- 1 root root    1387 Jul 10 11:54 gtm_test_install.out
# 		-rw-rw-rw- 1 root root  366080 Jul 10 11:54 mumps.dat
# 		-rw-r--r-- 1 root root    1536 Jul 10 11:54 mumps.gld
# 	test_gtm:
# 		-rw-r--r-- 1 root root  670 Jul 10 11:54 gtm.out
# 		-rw-r--r-- 1 root root 1536 Jul 10 11:54 mumps.gld
# 		-rw-r--r-- 1 root root   88 Jul 10 11:54 sim.m
# 		-rw-r--r-- 1 root root  869 Jul 10 11:54 sim.o
set msg2 = `ls -al * | grep -v \> | grep w- | wc -l`
@ msg2 = $msg2 - 7
if ( $msg2 ) then
    echo "Number of writeable lines = $msg2"		>>&! gtm_test_install.out
    ls -al * | grep -v \> | grep w-			>>&! gtm_test_install.out
endif

setenv gtm_dist .

gtm << EOF						>>&! gtm_test_install.out
write \$zchset
EOF

setenv gtm_dist utf8

setenv LD_LIBRARY_PATH $libpath
setenv LIBPATH $libpath
setenv gtm_chset utf-8
# depending on the list of locales configured, locale -a might be considered a binary output.
# grep needs -a option to process the output as text but -a is not supported on the non-linux servers we have.
if ( "linux" == "$arch" ) then
	set binaryopt = "-a"
else
	set binaryopt = ""
endif
set utflocale = `locale -a | grep $binaryopt -iE '\.utf.?8$' | head -n1`
setenv LC_ALL $utflocale

gtm << EOF						>>&! gtm_test_install.out
write \$zchset
EOF

setenv gtm_dist $save_gtm_dist/utf8
setenv gtmroutines ". $save_gtm_dist/utf8"

mkdir test_gtm_utf8
cd test_gtm_utf8
cp ../test_gtm/sim.m .

# set to test directory
setenv gtmdir $save_gtm_dist/test_gtm_utf8
# make gtm set the utf locale
unsetenv LC_CTYPE
if ($?gtm_icu_version) then
	# unset gtm_icu_version to test gtmprofile.gtc setting it
	setenv save_icu $gtm_icu_version
	unsetenv gtm_icu_version
endif
# NOTE: this is not the alias gtm, but the script sr_unix/gtm.gtc
../gtm -r sim >& gtm.out

# get $ZCHSET
echo ""						>>&! $save_gtm_dist/gtm_test_install.out
grep ZCHSET gtm.out				>>&! $save_gtm_dist/gtm_test_install.out
if ($?save_icu) then
	# restore saved gtm_icu_version
	setenv gtm_icu_version $save_icu
endif
# test gtmsecshr with an alternate user
set XCMD='do ^GTMHELP("",$ztrnlnm("gtm_dist")_"/gtmhelp.gld")'
if ($?gtm_icu_version) then
	set icuver = "gtm_icu_version=$gtm_icu_version"
else
	set icuver = ""
endif
su - gtmtest -c "env LD_LIBRARY_PATH=$libpath LC_ALL=$LC_ALL gtm_chset=UTF-8 $icuver gtm_dist=$gtm_dist gtmroutines='$gtmroutines' $gtm_dist/mumps -run %XCMD '${XCMD:q}' < /dev/null" > gtmtest.out   #BYPASSOKLENGTH
# if we see the 'Topic? ' prompt, all is well
grep -q '^Topic. $' gtmtest.out
if ( 0 == $status ) cat gtmtest.out			>>&! $save_gtm_dist/gtm_test_install.out
# get journal output
cd V*/g

$save_gtm_dist/mupip journal -extract -forward gtm.mjl					>& mupip.out
env gtmgbldir=$save_gtm_dist/test_gtm/mumps.gld $save_gtm_dist/mupip integ -reg "*"	>& integ.out
set mupip_status = $status
# output the change lines
echo ""						>>&! $save_gtm_dist/gtm_test_install.out
echo "Global changes in the gtm.mjl file:"	>>&! $save_gtm_dist/gtm_test_install.out
awk -F\\ '/=/{print ($NF)}' gtm.mjf		>>&! $save_gtm_dist/gtm_test_install.out
if (0 != $mupip_status) then
	echo "Error: mupip integ returned $status status instead of 0" >>&! $save_gtm_dist/gtm_test_install.out
else
	echo "mupip integ returned a 0 status - as expected" >>&! $save_gtm_dist/gtm_test_install.out
endif
$gtm_dist/gtmsecshr				>>&! $save_gtm_dist/gtm_test_install.out
$gtm_com/IGS $save_gtm_dist/gtmsecshr "STOP"	 # gtm_dist points to utf8/ and gtmsecshr is a softlink to $save_gtm_dist/gtmsecshr

echo "Build test the encryption plugin"									>>&! $save_gtm_dist/gtm_test_install.out #BYPASSOKLENGTH
mkdir -p $save_gtm_dist/plugin/buildcrypt
cd $save_gtm_dist/plugin/buildcrypt
tar xf $save_gtm_dist/plugin/gtmcrypt/source.tar
env LD_LIBRARY_PATH=$libpath LC_ALL=$LC_ALL GTMXC_gpgagent=$save_gtm_dist/plugin/gpgagent.tab sh -v README >& $save_gtm_dist/encr_plugin_build_test.out #BYPASSOKLENGTH
if ($status) echo "Encryption plugin build failure"
grep "GTM.[WIFE]" $save_gtm_dist/encr_plugin_build_test.out
setenv gtmroutines ". $gtm_dist/plugin/o($gtm_dist/plugin/r) $gtm_dist"
setenv gtm_passwd "`echo gtmrocks | env gtm_dist=$gtm_dist $gtm_dist/plugin/gtmcrypt/maskpass | cut -f3 -d ' '`"
setenv GTMXC_gpgagent $gtm_dist/plugin/gpgagent.tab
setenv gtm_pinentry_log $gtm_dist/encr_plugin_build_test.out
echo "Default gtmroutines"										>>&! $save_gtm_dist/gtm_test_install.out #BYPASSOKLENGTH
echo GETPIN | env LD_LIBRARY_PATH=$libpath LC_ALL=$LC_ALL $gtm_dist/plugin/gtmcrypt/pinentry-gtm.sh	>>&! $save_gtm_dist/gtm_test_install.out #BYPASSOKLENGTH
echo "Null gtmroutines"											>>&! $save_gtm_dist/gtm_test_install.out #BYPASSOKLENGTH
unsetenv gtmroutines
echo GETPIN | env LD_LIBRARY_PATH=$libpath LC_ALL=$LC_ALL $gtm_dist/plugin/gtmcrypt/pinentry-gtm.sh	>>&! $save_gtm_dist/gtm_test_install.out #BYPASSOKLENGTH

cd $save_gtm_dist

# strip off the copyright lines
tail -n +11 $gtm_tools/gtm_test_install.txt > gtm_test_install.txt
# remove white space at end of created output
sed 's/[ 	][ 	]*$//' gtm_test_install.out >&! gtm_test_install.out_stripspaces
cmp gtm_test_install.txt gtm_test_install.out_stripspaces
if ($status) then
	echo "---------"
	echo "GTM_TEST_INSTALL-E-ERROR the output is not as expected. Check $PWD"
	diff gtm_test_install.txt gtm_test_install.out_stripspaces
	set exitstat = 1
	echo "---------"
else
	echo "The test succeeded, the output is in $PWD/gtm_test_install.out"
	set exitstat = 0
endif

echo "------------------------------------------------------------------------"
exit $exitstat
