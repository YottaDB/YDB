#!/usr/local/bin/tcsh
#################################################################
#								#
#	Copyright 2011, 2013 Fidelity Information Services, Inc       #
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
# output the change lines
echo ""							>>&! $gtm_dist/gtm_test_install.out
echo "Global changes in the gtm.mjl file:"		>>&! $gtm_dist/gtm_test_install.out
grep = gtm.mjf | awk -F\\ '{print ($NF)}'		>>&! $gtm_dist/gtm_test_install.out
cat integ.out						>>&! $gtm_dist/gtm_test_install.out

cd $gtm_dist

# keep the utf8 libicu search code below in synch with configure.gtc!

set is64bit_gtm = `file mumps | grep 64 | wc -l`

# please keep in sync with sr_unix/set_library_path.csh
if ( $is64bit_gtm == 1 ) then
	set library_path = "/usr/local/lib64 /usr/local/lib /usr/lib64 /usr/lib/x86_64-linux-gnu /usr/lib"
else
	set library_path = "                 /usr/local/lib /usr/lib32 /usr/lib/i386-linux-gnu   /usr/lib"
endif

set is64bit_icu = 0
# Set the appropriate extensions for ICU libraries depending on the platforms
set icu_ext = ".so"
if ( $arch == "ibm" ) then
	set icu_ext = ".a"
else if ( $arch == "hp" ) then
	set icu_ext = ".sl"
endif

# Check the presence of gtm_icu_version
set gtm_icu_version_set = "FALSE"
if ($?gtm_icu_version) then
	if ("" != $gtm_icu_version) then
		set gtm_icu_version_set = "TRUE"
	endif
endif

foreach libpath ($library_path)
	set icu_lib_found = 0
	if ( "FALSE" == "$gtm_icu_version_set" && ( -f "$libpath/libicuio$icu_ext" ) ) then
		set icu_lib_found = 1
		# Find the actual version'ed library to which libicuio.{so,sl,a} points to
		set icu_versioned_lib = `ls -l $libpath/libicuio$icu_ext | awk '{print $NF}'`
		# Find out vital parameters
		if ( "$arch" == "ibm" || "$arch" == "zos" ) then
			# From the version'ed library(eg. libicuio36.0.a) extract out
			# 36.0.a
			set full_icu_ver_string = `echo $icu_versioned_lib | sed 's/libicuio//g'`
			# Extract 36 from 36.0.a
			set majmin=`echo $full_icu_ver_string | cut -f 1 -d '.'`
		else
			set full_icu_ver_string=`echo $icu_versioned_lib | sed 's/libicuio\.//g'`
			set majmin=`echo $full_icu_ver_string | cut -f 2 -d '.'`
		endif
	else if ( "TRUE" == "$gtm_icu_version_set" ) then
		set majmin = `echo $gtm_icu_version | sed 's/\.//'`
		if ( -f "$libpath/libicuio$majmin$icu_ext" || -f "$libpath/libicuio$icu_ext.$majmin" ) then
			set icu_lib_found = 1
		else
			set icu_lib_found = 0
		endif

	endif
	if ( $icu_lib_found ) then
		# Figure out the object mode(64 bit or 32 bit) of ICU libraries on the target machine
		if ( "linux" == "$arch" || "sun" == "$arch" || "solaris" == $arch ) then
			set icu_full_ver_lib = `sh -c "ls -l $libpath/libicuio$icu_ext.$majmin 2>/dev/null" | awk '{print $NF}'`
			set is64bit_icu = `sh -c "file $libpath/$icu_full_ver_lib 2>/dev/null | grep "64-bit" | wc -l"`
		else if ( "hp" == "$arch" ) then
			set icu_full_ver_lib = `sh -c "ls -l $libpath/libicuio$icu_ext.$majmin 2>/dev/null" | awk '{print $NF}'`
			set is64bit_icu = `sh -c "file $libpath/$icu_full_ver_lib 2>/dev/null | grep "IA64" | wc -l"`
		else if ( "ibm" == "$arch" ) then
			set icu_full_ver_lib = `sh -c "ls -l $libpath/libicuio$majmin$icu_ext 2>/dev/null" | awk '{print $NF}'`
			set is64bit_icu = `sh -c "nm -X64 $libpath/$icu_full_ver_lib 2>/dev/null | head -n 1 | wc -l"`
		else if ( "zos" == "$arch" ) then
			set icu_full_ver_lib = `sh -c "ls -l $libpath/libicuio$majmin$icu_ext 2>/dev/null" | awk '{print $NF}'`
			set is64bit_icu = `sh -c "file $libpath/$icu_full_ver_lib 2>/dev/null | grep "amode=64" | wc -l"`
		endif
		# Make sure both GTM and ICU are in sync with object mode compatibility (eg both are 32 bit/64 bit)
		if ( ( "$is64bit_gtm" == 1 ) && ( "$is64bit_icu" != 0 ) ) then
			set found_icu = 1
		else if ( ( "$is64bit_gtm" != 1 ) && ( "$is64bit_icu" == 0 ) ) then
			set found_icu = 1
		else
			set found_icu = 0
		endif
		if ( "$found_icu" == 1 && "$majmin" >= 36 ) then
			set save_icu_libpath = $libpath
			set majorver = `expr $majmin / 10`
			set minorver = `expr $majmin % 10`
			setenv gtm_icu_version "$majorver.$minorver"
			break
		endif
	endif
end
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

if ( -d utf8) then
	setenv LD_LIBRARY_PATH $libpath
	setenv LIBPATH $libpath
	setenv gtm_chset utf-8
	set utflocale = `locale -a | grep -i en_us | grep -i utf | sed 's/.lp64$//' | grep '8$' | head -n 1`
	if ( "OS/390" == `uname` ) then
		setenv gtm_chset_locale $utflocale
	else
		setenv LC_ALL $utflocale
	endif

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
	../gtm -r sim >& gtm.out

	# get $ZCHSET
	echo ""						>>&! $save_gtm_dist/gtm_test_install.out
	grep ZCHSET gtm.out				>>&! $save_gtm_dist/gtm_test_install.out
	# test gtmsecshr with an alternate user
	set XCMD='do ^GTMHELP("",$ztrnlnm("gtm_dist")_"/gtmhelp.gld")'
	su - gtmtest1 -c "env LD_LIBRARY_PATH=$libpath LC_ALL=$LC_ALL gtm_chset=UTF-8 gtm_dist=$gtm_dist gtmroutines='$gtmroutines' $gtm_dist/mumps -run %XCMD '${XCMD:q}' < /dev/null" > gtmtest.out   #BYPASSOK line length
	# if we see the 'Topic? ' prompt, all is well
	grep -q '^Topic. $' gtmtest.out
	if ( $status ) cat gtmtest.out			>>&! $save_gtm_dist/gtm_test_install.out
	# get journal output
	cd V*/g

	$save_gtm_dist/mupip journal -extract -forward gtm.mjl					>& mupip.out
	env gtmgbldir=$save_gtm_dist/test_gtm/mumps.gld $save_gtm_dist/mupip integ -reg "*"	>& integ.out

	# output the change lines
	echo ""						>>&! $save_gtm_dist/gtm_test_install.out
	echo "Global changes in the gtm.mjl file:"	>>&! $save_gtm_dist/gtm_test_install.out
	awk -F\\ '/=/{print ($NF)}' gtm.mjf		>>&! $save_gtm_dist/gtm_test_install.out
	cat integ.out					>>&! $save_gtm_dist/gtm_test_install.out
	$gtm_dist/gtmsecshr				>>&! $save_gtm_dist/gtm_test_install.out
	cd $save_gtm_dist

else
	# BEGIN - Fake the UTF-8 mode run for platforms that don't support it
cat <<EOF						>>&! gtm_test_install.out

GTM>
UTF-8
GTM>

ZCHSET= UTF-8

Global changes in the gtm.mjl file:
^a="1"
^b="2"


Integ of region DEFAULT

No errors detected by integ.

Type           Blocks         Records          % Used      Adjacent

Directory           2               3           0.756            NA
Index               2               2           0.585             2
Data                2               2           0.585             2
Free             4994              NA              NA            NA
Total            5000               7              NA             4
EOF
	#  END  - Fake the UTF-8 mode run for platforms that don't support it
endif

# strip off the copyright lines
tail -n +11 $gtm_tools/gtm_test_install.txt > gtm_test_install.txt
# remove white space at end of created output so it looks like version after ftpput is run
echo ':%s/[ 	][ 	]*$//g:wall\!:q' | vim -n gtm_test_install.out >& /dev/null

\diff gtm_test_install.txt gtm_test_install.out >& /dev/null
if ($status) then
	echo "---------"
	echo "GTM_TEST_INSTALL-E-ERROR the output is not as expected."
	\diff gtm_test_install.txt gtm_test_install.out
	set exitstat = 1
	echo "---------"
else
	echo "The test succeeded, the output is in "`pwd`/gtm_test_install.out
	set exitstat = 0
endif

echo "------------------------------------------------------------------------"
exit $exitstat
