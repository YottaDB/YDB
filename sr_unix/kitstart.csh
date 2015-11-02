#!/usr/local/bin/tcsh
#################################################################
#								#
#	Copyright 2011, 2012 Fidelity Information Services, Inc       #
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
# kitstart.csh creates distribution kits for pro and dbg.
# In order to test any configure.csh changes, copy a modified version into /usr/library/Vxxx/pro/configure
# and /usr/library/Vxxx/dbg/configure.  Then execute $gtm_tools/kitstart.csh -ti Vxxx and check output log.
# If changes are made to the configure script, such as removing files or changing permissions in the install
# directory there may need to be changes made to files used by kitstart.csh to execute
# $gtm_tools/gtm_compare_dist.csh.
#
# Make sure don't start in utf-8 mode
if ($?gtm_chset) then
	if (M != $gtm_chset) then
		set longline='$LC_CTYPE, $gtm_dist, and $gtmroutines for M mode'
		echo '$gtm_chset'" = $gtm_chset, so change to M mode and also check "$longline
		exit
	endif
endif

# This script needs root privileges to
# - test install GT.M
# - set file ownership to 40535
set euser = `$gtm_dist/geteuid`
if ("$euser" != "root") then
	echo "You must have root privileges to run kitstart"
	exit -1
endif

if (-e /etc/csh.cshrc) then
	# on lester (HPUX), /etc/csh.cshrc does not seem to be invoked at tcsh startup so invoke it explicitly
	# this is what defines the "version" alias used down below.
	source /etc/csh.cshrc
endif

# we need s_linux and s_linux64 here
source $cms_tools/cms_cshrc.csh

source $cms_tools/server_list
if ("$distrib_servers_unix" !~ *${HOST:r:r:r}*) then
	echo "This is not a distribution server. Exiting."
	exit
endif

setenv PATH "/usr/local/bin:/usr/sbin:/usr/ccs/bin:/usr/bin:/bin"

# get the osname and arch type from the tables in server_list

set servers      = ( $distrib_servers_unix )
set platformarch = ( $distrib_unix_platformarch )

foreach server ( $servers )
	@ index++
	if ("$server" =~ *${HOST:r:r:r}*) then
		set os_arch=$platformarch[$index]		# contortion alert! get the OS_ARCH value from the list
		set os_arch="${os_arch:s/_/ /}"			# and spilt OS_ARCH into "OS ARCH" and
		set os_arch=( ${os_arch:s/_/ /} )		# enclose it inside parenthesis to force conversion to an array
		break
	endif
end
set osname = $os_arch[1]
set arch = $os_arch[2]


set package = "tar cf"
set repackage = "tar rf"
set package_ext = "tar"
if ("$osname" == "os390") then
	set package = "pax -w -x pax -f"
	set repackage = "pax -a -f"
	set package_ext = "pax"
endif

set syntaxerr = 0
set arguments = "$argv"
if ($#argv < 1) then
	set syntaxerr = 1
else
	set testinstall = 0
	if ("$1" == "logfile") then
		set logfile = 1
		shift
	endif
	if ("$1" == "-ti") then
		set testinstall = 1
		shift
	endif
	if ("$1" == "" || "$2" != "" && "$2" != "pro" && "$2" != "dbg") then
		set syntaxerr = 1
	endif
endif

if ($syntaxerr) then
	echo ""
	echo "Usage : $0 [-ti] <ver> [pro | dbg]"
	echo ""
	echo "<ver>       : Version with no punctuations; create distribution of this GT.M version (must be in $gtm_root)"
	echo "-ti         : Test installation"
	echo "[pro | dbg] : Create distribution of this image; both if not specified"
	echo ""
	exit 1
endif

set version = "${1:au}"		# ':au' - 'a' means apply to the whole string and 'u' means uppercase everything

set imagetype = "pro dbg"
if ($2 != "") then
	set imagetype = $2
endif

if (! -d $gtm_root/$version) then
	echo ""
	echo "$gtm_root/$version does not exist"
	echo ""
	exit 2
endif

# make sure $gtm_tools version is same as indicated in $version
if ($gtm_tools:h:t != $version) then
	echo ""
	echo "$version selected for kit should equal $gtm_tools:h:t in gtm_tools"
	echo ""
	exit 1
endif

if (! $?logfile) then
	set fname = ${gtm_root}/$version/log/kitstart.`date +%Y%m%d%H%M`
	echo "output will be in $fname"
	$0 logfile $arguments >&! $fname
	set save_status = $status
	grep "Test of installation" $fname
	exit $save_status
endif
########################################################################################

version $version p  # Set the current version so that relative paths work
cmsver $version	    # Set appropriate path to locate $version sources in CMS, the default is V990
set releasever = `$gtm_dist/mumps -run %XCMD 'write $piece($zversion," ",2),!'`

# create a README.txt which has the current year in it
setenv readme_txt ${gtm_com}/README.txt
set year = `date +%Y`
sed "s/#YEAR#/$year/" $cms_tools/license_README_txt > $readme_txt
chmod 444 $readme_txt

# Set the open source flag and set lib_specific to the platform specific directories that needs to
# be copied as a part of open source distribution (down the script)
set open_source = 0
set GNU_COPYING_license = ""
set OPENSOURCE_build_README = ""
if ("$osname" == "linux" && ( "$arch" == "i686" || "x8664" == "$arch" )) then
	set open_source = 1
	set lib_specific = ($s_linux)
	if ("x8664" == "$arch" ) set lib_specific = ($s_linux64)

	set GNU_COPYING_license = "${gtm_com}/COPYING"
	/bin/cp -pf $cms_tools/opensource_COPYING $GNU_COPYING_license
	chmod 444 $GNU_COPYING_license

	# create README with current year in it
	set OPENSOURCE_build_README = "${gtm_com}/README"
	sed "s/#YEAR#/$year/" $cms_tools/opensource_README > $OPENSOURCE_build_README
	chmod 444 $OPENSOURCE_build_README
endif
if ("$osname" == "osf1" && "$arch" == "alpha") then
	set open_source = 1
	set lib_specific = "$s_dux"

	set GNU_COPYING_license = "${gtm_com}/COPYING"
	/bin/cp -pf $cms_tools/opensource_COPYING $GNU_COPYING_license
	chmod 444 $GNU_COPYING_license
endif

set product = "gtm"
set dist = "$gtm_ver/dist"
set tmp_dist = "$gtm_ver/tmp_dist"
set install = "$gtm_ver/install"
set dist_prefix = "${product}_${version}_${osname}_${arch}"
set notdistributed = '_*.o GDE*.m *.log map obj'
set utf8_notdistributed = '_*.o *.m *.log map obj [a-z]*'

if (-d $dist || -d $tmp_dist || -d $install) then
	echo ""
	echo "$dist or $tmp_dist or $install exists. Exiting..."
	exit 3
endif

echo ""
if ("$GNU_COPYING_license" != "") then
	if (! -r "$GNU_COPYING_license") then
		echo "Could not locate GNU Copying license at $GNU_COPYING_license. Exiting..."
		exit 4
	endif
	if ("$OPENSOURCE_build_README" != "") then
		if (! -r $OPENSOURCE_build_README) then
			echo "Could not locate Open Source Build README at $OPENSOURCE_build_README. Exiting..."
			exit 4
		endif
	endif
	set opensource_dist = "${dist}/opensource"
	echo "Creating $dist (for non open source customers) and $opensource_dist (for open source)"
	mkdir -p $opensource_dist || exit 4
else
	echo "Creating $dist"
	mkdir $dist || exit 4
endif

if ("$GNU_COPYING_license" != "") then
	if (! -e ${dist}/README) then
		cat > ${dist}/README << OPENSOURCE_EOF
For paying customers, distribute files in ${dist}
For non paying customers (Sourceforge) who use the Open Source version,
distribute files in ${opensource_dist}
The Open Source binary distribution includes the GNU License (file $GNU_COPYING_license:t)
The Open Source source distribution includes the GNU License (file $GNU_COPYING_license:t)
OPENSOURCE_EOF
		if ("$OPENSOURCE_build_README" != "") then
			cat >> ${dist}/README << OPENSOURCE_EOF
and the build procedure documentation (file $OPENSOURCE_build_README:t)
OPENSOURCE_EOF
		endif
		chmod a-xw ${dist}/README
	endif
endif
foreach image ($imagetype)
	echo ""
	echo "Creating ${tmp_dist}/${image}"
	mkdir -p ${tmp_dist}/${image} || exit 5
	cd ${tmp_dist}/${image} || exit 7
	echo ""
	echo "Copying files from ${gtm_ver}/${image}"
	if ("aix" == $osname) then
		/bin/cp -rh ${gtm_ver}/${image}/* . || exit 8
	else if ("solaris" == $osname) then
		/usr/local/bin/cp -r ${gtm_ver}/${image}/* . || exit 8
	else
		/bin/cp -r ${gtm_ver}/${image}/* . || exit 8
	endif
	echo ""
	echo "Removing files that are not distributed (${notdistributed})"
	/bin/rm -rf ${notdistributed} || exit 9
	if (-e utf8) then
		cd utf8
		/bin/rm -rf ${utf8_notdistributed} || exit 9
		cd ..
	endif
	# add the README.txt file
	cp $readme_txt README.txt || exit 9
	# add the custom_errors_sample.txt file
	cp $gtm_tools/custom_errors_sample.txt . || exit 9
	if (-e gtmsecshrdir) then
		$gtm_com/IGS gtmsecshr "UNHIDE"	# make root-owned gtmsecshrdir world-readable
		chmod u+w gtmsecshrdir
	endif
	if (-x dbcertify && -f V5CBSU.m) then
		set dist_file = "${dist}/dbcertify_${version}_${osname}_${arch}_${image}.${package_ext}"
		echo ""
		echo "Creating $dist_file"
		$package $dist_file README.txt dbcertify V5CBSU.m || exit 10
		echo "Gzipping $dist_file"
		gzip $dist_file || exit 11
		if ("$GNU_COPYING_license" != "") then
			echo ""
			echo "Creating dbcertify distribution for open source (includes GNU License)"
			echo ""
			echo "Copying $GNU_COPYING_license to $cwd"
			/bin/cp $GNU_COPYING_license . || exit 8
			set dist_file="${opensource_dist}/dbcertify_${version}_${osname}_${arch}_${image}.${package_ext}"
			echo ""
			echo "Creating $dist_file"
			$package $dist_file README.txt COPYING dbcertify V5CBSU.m || exit 10
			echo ""
			echo "Gzipping $dist_file"
			gzip $dist_file || exit 11
			rm -f COPYING || exit 9
		endif
		echo "Removing dbcertify"
		rm -f dbcertify V5CBSU.* || exit 9
		if (-e utf8) then
			cd utf8
			rm -f dbcertify V5CBSU.* || exit 9
			cd ..
		endif
	else
		echo ""
		echo "No dbcertify or V5CBSU.m"
	endif
	if (-e GTMDefinedTypesInit.m) then
		set dist_file = "${dist}/GTMDefinedTypesInit_${version}_${osname}_${arch}_${image}.${package_ext}"
		echo ""
		echo "Creating $dist_file"
		$package $dist_file README.txt GTMDefinedTypesInit.m || exit 10
		echo "Gzipping $dist_file"
		gzip $dist_file || exit 11
		if ("$GNU_COPYING_license" != "") then
			echo ""
			echo "Creating GTMDefinedTypesInit distribution for open source (includes GNU License)"
			echo ""
			echo "Copying $GNU_COPYING_license to $cwd"
			/bin/cp $GNU_COPYING_license . || exit 8
			set dist_file="${opensource_dist}/GTMDefinedTypesInit_${version}_${osname}_${arch}_${image}.${package_ext}"
			echo ""
			echo "Creating $dist_file"
			$package $dist_file README.txt COPYING GTMDefinedTypesInit.m || exit 10
			echo ""
			echo "Gzipping $dist_file"
			gzip $dist_file || exit 11
			rm -f COPYING || exit 9
		endif
		echo "Removing GTMDefinedTypesInit"
		rm -f GTMDefinedTypesInit.* || exit 9
		if (-e utf8) then
			cd utf8
			rm -f GTMDefinedTypesInit.* || exit 9
			cd ..
		endif
	else
		echo ""
		echo "No GTMDefinedTypesInit"
	endif
	set dist_file = "${dist}/${dist_prefix}_${image}.${package_ext}"
	# no files to be executable or writeable
	find . -type f -exec chmod a-xw {} \;
	# no directories to be writeable for group or world if aix or 32-bit linux, otherwise for all
	chmod a+x configure
	chmod a+x gtminstall
	if ((aix == ${osname}) || ((linux == ${osname}) && (i686 == "$arch"))) then
		find . -type d -exec chmod go-w {} \;
	else
		find . -type d -exec chmod a-w {} \;
	endif
	# use 40535 for owner and group
	find . -exec chown 40535:40535 {} \;
	echo ""
	echo "Creating $dist_file"
	if (("hpux" == ${osname})) then
		$package $dist_file . >& /dev/null
	else
		$package $dist_file . || exit 10
	endif
	echo ""
	echo "Gzipping $dist_file"
	gzip $dist_file || exit 11
	if ("$GNU_COPYING_license" != "") then
		echo ""
		echo "Creating distribution for open source (includes GNU License)"
		echo ""
		echo "Copying $GNU_COPYING_license to $cwd"
		/bin/cp $GNU_COPYING_license . || exit 8
		chown 40535:40535 COPYING
		set dist_file="${opensource_dist}/${dist_prefix}_${image}.${package_ext}"
		echo ""
		echo "Creating $dist_file"
		$package $dist_file . || exit 10
		echo ""
		echo "Gzipping $dist_file"
		gzip $dist_file || exit 11
		rm -f COPYING || exit 9
	endif
end
echo ""

# create src tar only for linux and tru64
if ("$GNU_COPYING_license" != "") then
	cd ${opensource_dist}
	if ("$OPENSOURCE_build_README" != "") then
		echo "Creating source distribution for Opensource including $GNU_COPYING_license:t and $OPENSOURCE_build_README:t"
	else
		echo "Creating source distribution for Opensource including $GNU_COPYING_license:t"
	endif
	echo ""
	# tar only the directories in ${liblist}
	set liblist = ""
	foreach libdir ($lib_specific)
		set liblist = "$liblist $libdir:t"
	end
	echo ""
	set src_tar="${opensource_dist}/${dist_prefix}_src.${package_ext}"
	echo "Creating $src_tar"

	echo "Copy in the original sources from ${version}"
	mkdir ${version}
	cp -r $lib_specific ./${version}/

	# comlist.mk builds fail on newer 32bit versions of RHEL6 and Ubuntu
	# 12.04 due to a bad interaction between the deprecated -I- option and
	# GCC. See mails with the subject:
	# 	[GTM-6465] [cmake] #include "" vs #include <>
	# Keep in sync with test/manually_start/u_inref/makebuild.csh
	echo "Massage the source files so that we can build on i386 Linux and other platforms without -I-"
	set hdrlist="emit_code_sp.h|rtnhdr.h|auto_zlink.h|make_mode_sp.h|auto_zlink_sp.h|emit_code.h|mdefsp.h|incr_link_sp.h|gtm_mtio.h|obj_filesp.h|zbreaksp.h|gtm_registers.h|opcode_def.h"  #BYPASSOK line length
	set sedlist=${hdrlist:as/|/ /:as/ /\|/} # fixing for use with SED requires some contortions - replace | with space and then space with \|
	grep -rlE "#include .(${hdrlist})." sr_* > changefiles.list
	foreach file (`cat changefiles.list`)
		set orig=${file:h}/.${file:t}
		mv ${file} {$orig}
		sed "s/#include .\(${sedlist}\)./#include <\1>/g" ${orig} > ${file}
		diff -u ${orig} ${file}
		rm ${orig}
	end

	echo "Copy in the generated files"
	set srdir = sr_${arch:s/i686/i386/:s/x8664/x86_64/}
	cp ${gtm_ver}/src/ttt.c ${gtm_ver}/src/*_ctl.c ${gtm_ver}/inc/merrors_ansi.h ./$version/$srdir/ || exit 10

	echo "Packaging the source from $version"
	cd $version || exit 10
	# Linux uses CMakeLists.txt, tru64 uses comlist.mk
	if ("linux" == "$osname") then
		# this lets the build override $cms_ver/sr_unix/CMakeLists.txt
		sed "s/GTM_RELEASE_VERSION/${releasever}/" ${gtm_ver}/tools/CMakeLists.txt > CMakeLists.txt || exit 10
		set liblist = "$liblist CMakeLists.txt"
	endif
	find . -exec chown 40535:40535 {} \;
	$package $src_tar $liblist || exit 10

	cd ${opensource_dist}
	rm -rf ./$version

	echo "Package the license and readme files"
	cd $gtm_com || exit 10
	if ("$OPENSOURCE_build_README" != "") then
		$repackage $src_tar $GNU_COPYING_license:t $OPENSOURCE_build_README:t || exit 10
	else
		$repackage $src_tar $GNU_COPYING_license:t $readme_txt:t || exit 10
	endif

	echo ""
	echo "Gzipping $src_tar"
	gzip $src_tar || exit 11
	echo ""
	if ("$OPENSOURCE_build_README" != "") then
		cat << EOF_MK
############################################################################
!!!!! TEST THE MAKEFILE !!!!!
First untar the opensource files in a directory:
mkdir ${opensource_dist}/build
cd ${opensource_dist}/build
tar zxvf $src_tar
Then follow the instructions from $gtm_com/README:
----------------------------------------------------------------------------
`cat $gtm_com/README`
############################################################################
EOF_MK
	endif
endif

find $dist -type f -exec chmod 444 {} \;
find $dist -type d -exec chmod 755 {} \;
chown -R library:gtc $dist
echo "Files in $dist"
/bin/ls -lR $dist
echo ""

set leavedir = 0
set kitver = ${gtm_ver:t:s/V//}

if ($testinstall) then
	echo ""
	echo "Testing installation"
	echo ""
	echo "Creating $install"
	mkdir ${install} || exit 12
	foreach image ($imagetype)
		echo ""
		echo "Testing installation for $image"
		cd ${tmp_dist}/${image} || exit 13
		# V54000 introduced tests for installation validity.  The post v54000(includes it) creates an installation
		# for comparison using gtm_compare_build.csh later, but this is based on a restricted group
		# installation.  We now include an unrestricted group installation into the "${install}/defgroup" directory
		# for automated testing later. This will also verify the correct gtmsecshr permissions for both types
		# of installation. We answer "n" to the last question to remove files since we need them for the
		# restricted build.
		# V54002 now asks for an installation group (newline entered for default) so response needs one more
		# blank line for default group
		# V54003 now asks whether or not to retain .o files if libgtmutil.so is created
		# We answer "y" to this question
		# If libgtmutil.so is not created(on i686) this question is not asked
		if ("$osname" != "osf1") then
			if ("$osname" == "linux" && "$arch" == "i686") then
				sh ./configure << CONFIGURE_EOF


n
${install}/defgroup/${image}
y
y
n
n
n
CONFIGURE_EOF
			else
				sh ./configure << CONFIGURE_EOF


n
${install}/defgroup/${image}
y
y
n
n
y
n
CONFIGURE_EOF

			endif
		else
			sh ./configure << CONFIGURE_EOF


n
${install}/defgroup/${image}
y
n
y
n
CONFIGURE_EOF
		endif

		# We need for root to be a member of the restricted group.  It is a member of the "root" group
		# for all linux OS and it is a member of "lp" for all others except osf1 where it is "vboxusers"
		if("$osname" == "linux") then
			setenv rootgroup "root"
		else if ("$osname" != "osf1") then
			setenv rootgroup "lp"
		else
			setenv rootgroup "vboxusers"
		endif
		# V54002 now asks for an installation group before the restricted group question so response is
		# reversed from V54000
		# V54003 now asks whether or not to retain .o files if libgtmutil.so is created
		# We answer "y" to this question
		# If libgtmutil.so is not created(on i686) this question is not asked
		if("$osname" != "osf1") then
			if ("$osname" == "linux" && "$arch" == "i686") then
	 			sh ./configure << CONFIGURE_EOF

$rootgroup
y
${install}/${image}
y
y
n
n
y
CONFIGURE_EOF
			else
	 			sh ./configure << CONFIGURE_EOF

$rootgroup
y
${install}/${image}
y
y
n
n
y
y
CONFIGURE_EOF
			endif
		else
			sh ./configure << CONFIGURE_EOF

$rootgroup
y
${install}/${image}
y
n
y
y
CONFIGURE_EOF
		endif

		# exit if the installation of the image failed
		if ($status) then
			echo ""
			echo "Installation of $image failed; configure returned error"
			exit 14
		endif
		if ("`/bin/ls`" != "") then
			echo ""
			echo "Installation of $image failed; leftover files in ${tmp_dist}/${image}"
			/bin/ls -l
			exit 15
		endif

		# compare the files and directories in the installation to those in the build
		if ("pro" == ${image}) then
			# create the build.dir.  Only have to do it once
			cd $gtm_ver || exit 14
			if ((${osname} != linux) && (${osname} != solaris)) echo pro: > ${tmp_dist}/build.dir
			ls -lR pro >> ${tmp_dist}/build.dir
			if (aix == ${osname}) then
				cat ${tmp_dist}/build.dir | \
awk '$0 == "pro/gtmsecshrdir:" {printf "\n%s\n", $0} $0 != "pro/gtmsecshrdir:" {printf "%s\n", $0}' > ${tmp_dist}/tbuild.dir
				mv ${tmp_dist}/tbuild.dir ${tmp_dist}/build.dir
			endif

			# make a defgroup directory under ${tmp_dist} and copy in the build.dir for use in
			# first iteration of the while loop
			mkdir ${tmp_dist}/defgroup
			cp ${tmp_dist}/build.dir ${tmp_dist}/defgroup

			set defgroup = "defgroup"
			@ both = 0
			while (2 > $both)
				# create the install.dir from both installations
				cd ${install}/$defgroup
				if ((${osname} != linux) && (${osname} != solaris)) echo pro: > ${tmp_dist}/$defgroup/install.dir
				ls -lR pro >> ${tmp_dist}/$defgroup/install.dir
				if (aix == ${osname}) then
					cat ${tmp_dist}/$defgroup/install.dir | \
awk '$0 == "pro/gtmsecshrdir:" {printf "\n%s\n", $0} $0 != "pro/gtmsecshrdir:" {printf "%s\n", $0}' > \
${tmp_dist}/$defgroup/tinstall.dir
					mv ${tmp_dist}/$defgroup/tinstall.dir ${tmp_dist}/$defgroup/install.dir
				endif
				cd ${tmp_dist}/${image}
				set comp="$gtm_tools/gtm_compare_dir.csh ${install} ${tmp_dist}/$defgroup $gtm_tools/bdelete.txt"
				if (("linux" == ${osname}) && ("i686" == ${arch})) then
					$comp $gtm_tools/linuxi686_badd.txt $gtm_tools/bdeldir.txt ${osname}
					set teststat = $status
				else if (("hpux" == ${osname}) && ("parisc" == ${arch})) then
					$comp $gtm_tools/hpuxparisc_badd.txt $gtm_tools/hpuxparisc_bdeldir.txt ${osname}
					set teststat = $status
				else if (("hpux" == ${osname}) && ("ia64" == ${arch})) then
					$comp $gtm_tools/hpuxia64_badd.txt $gtm_tools/bdeldir.txt ${osname}
					set teststat = $status
				else if (("osf1" == ${osname}) && ("alpha" == ${arch})) then
					$comp $gtm_tools/osf1alpha_badd.txt $gtm_tools/hpuxparisc_bdeldir.txt ${osname}
					set teststat = $status
				else
					$comp $gtm_tools/badd.txt $gtm_tools/bdeldir.txt ${osname}
					set teststat = $status
				endif
				if ($teststat) then
					echo ""
					echo "Comparison of build and install directories failed."
					echo "Look in ${tmp_dist}/$defgroup/dircompare/diff.out"
					exit 16
				endif
				# to simplify the code to do the gtm_compare_dir.csh for both restricted and unrestricted group
				# installations in a loop, we set defgroup to null for the second pass.  This takes advantage of
				# the unix/linux path interpretation where dir//subdir is the same as dir/subdir
				set defgroup = ""
				@ both = $both + 1
			end
		endif

		# test the default group installation
		$gtm_tools/gtm_test_install.csh ${install}/defgroup/${image}
		set teststat = $status
		echo ""
		echo ""
		if (! $teststat) then
			echo "Test of installation for default group ${version}/defgroup/${image} PASSED"
		else
			echo "Test of installation for default group ${version}/defgroup/${image} FAILED"
			set leavedir = 1
		endif

		# test the group restricted installation
		$gtm_tools/gtm_test_install.csh ${install}/${image}
		set teststat = $status
		echo ""
		echo ""
		if (! $teststat) then
			echo "Test of installation for group restricted ${version}/${image} PASSED"
		else
			echo "Test of installation for group restriced ${version}/${image} FAILED"
			set leavedir = 1
		endif

	end
endif

cd $gtm_ver || exit 16

if (! $leavedir) then
	echo ""
	echo "Removing temporary directories"
	echo ""
	echo ""
	/bin/rm -rf ${tmp_dist} ${install}
	exit 0
endif

echo ""
echo "Distribution creation/testing failed. Leaving directories ${tmp_dist} ${install}"
exit 17
