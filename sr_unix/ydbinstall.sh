#!/bin/sh -
#################################################################
# Copyright (c) 2014-2020 Fidelity National Information         #
# Services, Inc. and/or its subsidiaries. All rights reserved.  #
#								#
# Copyright (c) 2017-2023 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2018 Stephen L Johnson.				#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license. If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# ---------------------------------------------------------------------------------------------------------------------
# For the latest version of this script, run the following command
#	wget https://download.yottadb.com/ydbinstall.sh
# ---------------------------------------------------------------------------------------------------------------------
#
# This script automates the installation of YottaDB as much as possible,
# to the extent of attempting to download the distribution file.
#
# Note: This needs to be run as root.

# Function Definitions

#Append arguments in $2 to string named by $1 if not already in it
append_to_str()
{
	for substr in $2 ; do
		eval "echo \$$1" | grep -qsw $substr || eval "$1=\"\$$1 $substr\""
	done
}

# Function for required header file check
# As gcc terminates after the first header file not found error, the headers
# must be checked for individually.
check_if_hdrs_exist()
{
	hdrflag=1
	unset hdrnotfound
	tmpprog=$(mktemp -t ydbinstallXXXXXX --suffix=.c)
	for hdr in $hdrlist ; do
		printf "%s\n" '#include <stddef.h>' '#include <stdarg.h>' '#include <stdio.h>' \
			'#include <setjmp.h>' '#include "'$hdr'"' "int main() {}" >$tmpprog
		if ! gcc -H --syntax-only $tmpprog 1>/dev/null 2>&1 ; then
			hdrnotfound="$hdrnotfound $hdr" ; unset hdrflag
		fi
	done
	\rm -f $tmpprog
	if [ -z "$hdrflag" ] ; then
		printf "%s" "$1" >&2
		for shlib in $hdrnotfound ; do
			printf "%s" " $shlib" >&2
		done
		echo >&2
		unset alldepflag
	fi
}

# Function for required shared library check
check_if_shlibs_exist()
{
	shlibflag=1
	tmpliblist=$(mktemp -t ydbinstallXXXXXX --suffix=.txt)
	$ldconfig -p >$tmpliblist
	unset shlibnotfound
	for shlib in $shliblist ; do
		grep -Eqsw "$shlib"'[.[:digit:]]*' $tmpliblist || { shlibnotfound="$shlibnotfound $shlib" ; unset shlibflag ; }
	done
	\rm $tmpliblist
	if [ -z "$shlibflag" ] ; then
		printf "%s" "$1" >&2
		for shlib in $shlibnotfound ; do
			printf "%s" " $shlib" >&2
		done
		echo >&2
		unset alldepflag
	fi
}

# Function for required utility check
check_if_utils_exist()
{
	utilflag=1
	unset utilnotfound
	for util in $utillist ; do
		command -v $util >/dev/null 2>&1 || command -v /sbin/$util >/dev/null 2>&1 || { utilnotfound="$utilnotfound $util" ; unset utilflag ; }
	done
	if [ -z "$utilflag" ] ; then
		printf "%s" "$1" >&2
		for util in $utilnotfound ; do
			printf "%s" " $util" >&2
		done
		echo >&2
		unset alldepflag
	fi
}

# This function ensures that a target directory exists
dirensure()
{
	if [ ! -d "$1" ] ; then
		mkdir -p "$1"
		if [ "Y" = "$gtm_verbose" ] ; then echo Directory "$1" created ; fi
	fi
}

dump_info()
{
	set +x
	if [ -n "$timestamp" ] ; then echo timestamp " : " $timestamp ; fi
	if [ -n "$ydb_aim" ] ; then echo ydb_aim " : " $ydb_aim ; fi
	if [ -n "$ydb_branch" ] ; then echo ydb_branch " : " $ydb_branch ; fi
	if [ -n "$ydb_change_removeipc" ] ; then echo ydb_change_removeipc " : " $ydb_change_removeipc ; fi
	if [ -n "$ydb_debug" ] ; then echo ydb_debug " : " $ydb_debug ; fi
	if [ -n "$ydb_deprecated" ] ; then echo ydb_deprecated " : " $ydb_deprecated ; fi
	if [ -n "$ydb_distrib" ] ; then echo ydb_distrib " : " $ydb_distrib ; fi
	if [ -n "$ydb_dist" ] ; then echo ydb_dist " : " $ydb_dist ; fi
	if [ -n "$ydb_encplugin" ] ; then echo ydb_encplugin " : " $ydb_encplugin ; fi
	if [ -n "$ydb_filename" ] ; then echo ydb_filename " : " $ydb_filename ; fi
	if [ -n "$ydb_flavor" ] ; then echo ydb_flavor " : " $ydb_flavor ; fi
	if [ -n "$ydb_force_install" ] ; then echo ydb_force_install " : " $ydb_force_install ; fi
	if [ -n "$ydb_from_source" ] ; then echo ydb_from_source " : " $ydb_from_source ; fi
	if [ -n "$ydb_gui" ] ; then echo ydb_gui " : " $ydb_gui ; fi
	if [ -n "$ydb_installdir" ] ; then echo ydb_installdir " : " $ydb_installdir ; fi
	if [ -n "$ydb_octo" ] ; then echo ydb_octo " : " $ydb_octo ; fi
	if [ -n "$ydb_pkgconfig" ] ; then echo ydb_pkgconfig " : " $ydb_pkgconfig ; fi
	if [ -n "$ydb_posix" ] ; then echo ydb_posix " : " $ydb_posix ; fi
	if [ -n "$ydb_routines" ] ; then echo ydb_routines " : " $ydb_routines ; fi
	if [ -n "$ydb_utf8" ] ; then echo ydb_utf8 " : " $ydb_utf8 ; fi
	if [ -n "$ydb_version" ] ; then echo ydb_version " : " $ydb_version ; fi
	if [ -n "$ydb_zlib" ] ; then echo ydb_zlib " : " $ydb_zlib ; fi
	if [ -n "$gtm_arch" ] ; then echo gtm_arch " : " $gtm_arch ; fi
	if [ -n "$gtm_buildtype" ] ; then echo gtm_buildtype " : " $gtm_buildtype ; fi
	if [ -n "$gtm_configure_in" ] ; then echo gtm_configure_in " : " $gtm_configure_in ; fi
	if [ -n "$gtm_copyenv" ] ; then echo gtm_copyenv " : " $gtm_copyenv ; fi
	if [ -n "$gtm_copyexec" ] ; then echo gtm_copyexec " : " $gtm_copyexec ; fi
	if [ -n "$gtm_dryrun" ] ; then echo gtm_dryrun " : " $gtm_dryrun ; fi
	if [ -n "$gtm_ftp_dirname" ] ; then echo gtm_ftp_dirname " : " $gtm_ftp_dirname ; fi
	if [ -n "$gtm_group_already" ] ; then echo gtm_group_already " : " $gtm_group_already ; fi
	if [ -n "$gtm_group_restriction" ] ; then echo gtm_group_restriction " : " $gtm_group_restriction ; fi
	if [ -n "$gtm_group" ] ; then echo gtm_group " : " $gtm_group ; fi
	if [ -n "$gtm_gtm" ] ; then echo gtm_gtm " : " $gtm_gtm ; fi
	if [ -n "$gtm_hostos" ] ; then echo gtm_hostos " : " $gtm_hostos ; fi
	if [ -n "$gtm_install_flavor" ] ; then echo gtm_install_flavor " : " $gtm_install_flavor ; fi
	if [ -n "$gtm_keep_obj" ] ; then echo gtm_keep_obj " : " $gtm_keep_obj ; fi
	if [ -n "$gtm_lcase_utils" ] ; then echo gtm_lcase_utils " : " $gtm_lcase_utils ; fi
	if [ -n "$gtm_linkenv" ] ; then echo gtm_linkenv " : " $gtm_linkenv ; fi
	if [ -n "$gtm_linkexec" ] ; then echo gtm_linkexec " : " $gtm_linkexec ; fi
	if [ -n "$gtm_overwrite_existing" ] ; then echo gtm_overwrite_existing " : " $gtm_overwrite_existing ; fi
	if [ -n "$gtm_prompt_for_group" ] ; then echo gtm_prompt_for_group " : " $gtm_prompt_for_group ; fi
	if [ -n "$gtm_sf_dirname" ] ; then echo gtm_sf_dirname " : " $gtm_sf_dirname ; fi
	if [ -n "$gtm_tmpdir" ] ; then echo gtm_tmpdir " : " $gtm_tmpdir ; fi
	if [ -n "$gtm_user" ] ; then echo gtm_user " : " $gtm_user ; fi
	if [ -n "$gtm_verbose" ] ; then echo gtm_verbose " : " $gtm_verbose ; fi
	if [ "Y" = "$ydb_debug" ] ; then set -x ; fi
}

err_exit()
{
	echo "YottaDB installation aborted due to above error. Run `basename $0` --help for detailed option list"
	exit 1
}

# Output helpful information and exit. Note that tabulation in the file appears inconsistent, but
# is required for consistent tabulation on output. Reasons are unclear, but what is below works.
help_exit()
{
	set +x
	echo "ydbinstall [option] ... [version]"
	echo "Options are:"
	echo "--aim				-> installs AIM plugin"
	echo "--branch branchname		-> builds YottaDB from a specific git branch; use with --from-source"
	echo "--build-type buildtype		-> type of YottaDB build, default is pro"
	echo "--copyenv [dirname]		-> copy ydb_env_set, ydb_env_unset, and gtmprofile files to dirname, default /usr/local/etc; incompatible with linkenv"
	echo "--copyexec [dirname]		-> copy ydb & gtm scripts to dirname, default /usr/local/bin; incompatible with linkexec"
	echo "--debug				-> turn on debugging with set -x"
	echo "--distrib dirname or URL	-> source directory for YottaDB/GT.M distribution tarball, local or remote"
	echo "--dry-run			-> do everything short of installing YottaDB, including downloading the distribution"
	echo "--encplugin			-> compile and install the encryption plugin"
	echo "--filename filename		-> name of YottaDB distribution tarball"
	echo "--force-install			-> install even if the current platform is not Supported"
	echo "--from-source repo		-> builds and installs YottaDB from a git repo; defaults to building the latest master from gitlab if not specified; check README for list of prerequisites to build from source"
	echo "--group group			-> group that should own the YottaDB installation"
	echo "--group-restriction		-> limit execution to a group; defaults to unlimited if not specified"
	echo "--gtm				-> install GT.M instead of YottaDB"
	echo "--gui				-> download and install the YottaDB GUI"
	echo "--help				-> print this usage information"
	echo "--installdir dirname		-> directory where YottaDB is to be installed; defaults to /usr/local/lib/yottadb/version"
	echo "--keep-obj			-> keep .o files of M routines (normally deleted on platforms with YottaDB support for routines in shared libraries)"
	echo "--linkenv [dirname]		-> create link in dirname to ydb_env_set, ydb_env_unset & gtmprofile files, default /usr/local/etc; incompatible with copyenv"
	echo "--linkexec [dirname]		-> create link in dirname to ydb & gtm scripts, default /usr/local/bin; incompatible with copyexec"
	echo "--nocopyenv			-> do not copy ydb_env_set, ydb_env_unset, and gtmprofile to another directory"
	echo "--nocopyexec			-> do not copy ydb & gtm scripts to another directory"
	echo "--nodeprecated			-> do not install deprecated components, specifically %DSEWRAP"
	echo "--nolinkenv			-> do not create link to ydb_env_set, ydb_env_unset, and gtmprofile from another directory"
	echo "--nolinkexec			-> do not create link to ydb & gtm scripts from another directory"
	echo "--nopkg-config			-> do not create yottadb.pc for pkg-config, or update an existing file"
	echo "--octo parameters		-> download and install Octo; also installs required POSIX and AIM plugins. Specify optional cmake parameters for Octo as necessary"
	echo "--overwrite-existing		-> install into an existing directory, overwriting contents; defaults to requiring new directory"
	echo "--plugins-only			-> just install plugins for an existing YottaDB installation, not YottaDB"
	echo "--posix				-> download and install the POSIX plugin"
	echo "--preserveRemoveIPC		-> do not allow changes to RemoveIPC in /etc/systemd/login.conf if needed; defaults to allow changes"
	echo "--prompt-for-group		-> YottaDB installation script will prompt for group; default is yes for production releases V5.4-002 or later, no for all others"
	echo "--sodium			-> download and install the libsodium plugin"
	echo "--ucaseonly-utils		-> install only upper case utility program names; defaults to both if not specified"
	echo "--user username			-> user who should own YottaDB installation; default is root"
	echo "--utf8				-> install UTF-8 support"
	echo "--verbose			-> output diagnostic information as the script executes; default is to run quietly"
	echo "--zlib				-> download and install the zlib plugin"
	echo "Options that take a value (e.g, --group) can be specified as either --option=value or --option value."
	echo "Options marked with \"*\" are likely to be of interest primarily to YottaDB developers."
	echo "Version is defaulted from yottadb file if one exists in the same directory as the installer."
	echo "This version must run as root."
	echo ""
	echo "Example usages are (assumes latest YottaDB release is r1.38 and latest GT.M version is V7.0-005)"
	echo "	$0				# installs latest YottaDB release (r1.38) at /usr/local/lib/yottadb/r138"
	echo "	$0 --utf8			# installs YottaDB release r1.38 with added support for UTF-8"
	echo "	$0 --installdir /r138 r1.38	# installs YottaDB r1.38 at /r138"
	echo "	$0 --gtm			# installs latest GT.M version (V7.0-005) at /usr/local/lib/fis-gtm/V7.0-005_x86_64"
	echo ""
	echo "As options are processed left to right, later options can override earlier options."
	echo ""
	exit
}

# This function gets the OS id from the file passed in as $1 (either /etc/os-release or ../build_os_release)
# It also does minor adjustments (e.g. SLES, SLED, and openSUSE Leap are all reported as "sle").
getosid()
{
	osid=`grep -w ID $1 | cut -d= -f2 | cut -d'"' -f2`
	# Treat SLES (Server), SLED (Desktop) and OpenSUSE Leap distributions as the same.
	if [ "sled" = "$osid" ] || [ "sles" = "$osid" ] || [ "opensuse-leap" = "$osid" ] ; then
		osid="sle"
	# If the distribution is not Debian, RHEL, or Ubuntu (e.g., Linux Mint), see what it is like.
	elif ! echo "debian,rhel,ubuntu" | grep -qw $osid ; then
		osid=`grep -w ID_LIKE $1 | cut -d= -f2 | cut -d'"' -f2 | cut -d' ' -f1`
	fi
	echo $osid
}

# This function finds the current ICU version using ldconfig.
# See comment in sr_unix/configure.gtc for why we use ldconfig and not pkg-config.
# If file name is "libicuio.so.70", the below will return "70".
# If file name is "libicuio.so.suse65.1", the below will return "65.1.suse" (needed for YottaDB to work on SLED 15).
# There is a M version of this function in sr_unix/ydbenv.mpt as well as a .sh version in sr_unix/configure.gtc
# They need to be maintained in parallel to this function.
icu_version()
{
	$ldconfig -p | grep -m1 -F libicuio.so. | cut -d" " -f1 | sed 's/.*libicuio.so.\([a-z]*\)\([0-9\.]*\)/\2.\1/;s/\.$//;'
}

# This function installs the selected plugins. Before calling it, $ydb_installdir and $tmpdir need to be set so that it can
# find the right place to install the plugins and the right place to build the plugins respectively. This function will
# set remove_tmpdir to 0 if one or more plugin builds fail. The order of the plugins is alphabetical in two groups: those
# without dependencies, and a second group that has dependencies on plugins in the first group.

# This function is invoked whenever we detect an option that requires a value (e.g. --utf8) that is not
# immediately followed by a =. In that case, the next parameter in the command line ($2) is the value.
# We check if this parameter starts with a "--" as well and if so it denotes a different option and not a value.
#
# Input
# -----
# $1 for this function is the # of parameters remaining to be processed in command line
# $2 for this function is the next parameter in the command line immediately after the current option (which has a -- prefix).
#
# Output
# ------
# returns 0 in case $2 is non-null and does not start with a "--"
# returns 1 otherwise.
#
isvaluevalid()
{
	if [ 1 -lt "$1" ] ; then
		# bash might have a better way for checking whether $2 starts with "--" than the grep done below
		# but we want this script to run with sh so go with the approach that will work on all shells.
		retval=`echo "$2" | grep -c '^--'`
	else
		# option (e.g. --utf8) is followed by no other parameters in the command line
		retval=1
	fi
	echo $retval
}

install_plugins()
{
	if [ "Y" = $ydb_aim ] ; then install_std_plugin Util YDBAIM ; fi

	if [ "Y" = $ydb_encplugin ] ; then
		echo "Now installing YDBEncrypt"
		cd $tmpdir	# Get back to top level temporary directory as the current directory
		mkdir enc_tmp
		cd enc_tmp
		export ydb_dist=${ydb_installdir}
		if wget ${wget_flags} ${PWD} https://gitlab.com/YottaDB/Util/YDBEncrypt/-/archive/master/YDBEncrypt-master.tar.gz 2>enc.err 1>enc.out; then
			tar xzf YDBEncrypt-master.tar.gz
			cd YDBEncrypt-master
			if make -j `grep -c ^processor /proc/cpuinfo` 2>>../enc.err 1>>../enc.out && make install 2>>../enc.err 1>>../enc.out; then
				# Save the build directory if the make install command returns a non-zero exit code. Otherwise, remove it.
				if [ "Y" = "$gtm_verbose" ] ; then cat ../enc.err ../enc.out ; fi
				cd ../..
				\rm -R enc_tmp
				# rename gtmcrypt to ydbcrypt and create a symbolic link for backward compatibility
				mv ${ydb_installdir}/plugin/gtmcrypt ${ydb_installdir}/plugin/ydbcrypt
				ln -s ${ydb_installdir}/plugin/ydbcrypt ${ydb_installdir}/plugin/gtmcrypt
				# Enable execute permissions on .sh scripts in the YDBEncrypt plugin
				chmod +x ${ydb_installdir}/plugin/ydbcrypt/*.sh
			else
				echo "YDBEncrypt build failed. The build directory ($PWD) has been saved."
				remove_tmpdir=0
			fi
		else
			echo "Unable to download YDBEncrypt. Your internet connection and/or the gitlab servers may be down. Please try again later."
			remove_tmpdir=0
		fi
	fi

	if [ "Y" = "$ydb_posix" ] ; then install_std_plugin Util YDBPosix ; fi

	if [ "Y" = "$ydb_sodium" ] ; then install_std_plugin Util YDBSodium ; fi

	if [ "Y" = "$ydb_gui" ] ; then install_std_plugin UI YDBGUI ; fi

	if [ "Y" = $ydb_octo ] ; then install_std_plugin DBMS YDBOcto ; fi

	if [ "Y" = $ydb_zlib ] ; then
		echo "Now installing YDBZlib"
		cd $tmpdir	# Get back to top level temporary directory as the current directory
		mkdir zlib_tmp
		cd zlib_tmp
		if wget ${wget_flags} ${PWD} https://gitlab.com/YottaDB/Util/YDBZlib/-/archive/master/YDBZlib-master.tar.gz 1>zlib.log 2>&1; then
			tar xzf YDBZlib-master.tar.gz
			cd YDBZlib-master
			if gcc -c -fPIC -I${ydb_installdir} gtmzlib.c && gcc -o libgtmzlib.so -shared gtmzlib.o 1>>../zlib.log 2>&1; then
				# Save the build directory if either of the gcc commands return a non-zero exit code. Otherwise, remove it.
				if [ "Y" = "$gtm_verbose" ] ; then cat ../zlib.log ; fi
				cp gtmzlib.xc libgtmzlib.so ${ydb_installdir}/plugin
				cp _ZLIB.m ${ydb_installdir}/plugin/r
				if [ "Y" = $ydb_utf8 ] ; then
					mkdir utf8
					(
						cd utf8
						export ydb_chset="UTF-8"
						${ydb_installdir}/mumps ${ydb_installdir}/plugin/r/_ZLIB
						cp _ZLIB.o ${ydb_installdir}/plugin/o/utf8
					)
				fi
				${ydb_installdir}/mumps ${ydb_installdir}/plugin/r/_ZLIB
				cp _ZLIB.o ${ydb_installdir}/plugin/o
				cd ../..
				\rm -R zlib_tmp
			else
				echo "YDBZlib build failed. The build directory ($PWD/zlib_tmp) has been saved."
				remove_tmpdir=0
			fi
		else
			echo "Unable to download YDBZlib. Your internet connection and/or the gitlab servers may be down. Please try again later."
			remove_tmpdir=0
		fi
	fi
}

# Install a standard plugin.
# - $1 is the subdirectory of https://gitlab.com/YottaDB where the plugin is located
# - $2 is the name of the plugin
install_std_plugin()
{
	echo "Now installing $2"
	cd $tmpdir
	mkdir ${2}_tmp ; cd ${2}_tmp
	export ydb_dist=${ydb_installdir}
	if git clone --depth 1 https://gitlab.com/YottaDB/$1/$2.git $2-master 2>${2}.err 1>${2}.out ; then
		mkdir ${2}-master/build && cd ${2}-master/build
		# Build the plugin, saving the directory if the build fails
		if ( ${cmakecmd} .. && ${cmakecmd} --build . -j $(getconf _NPROCESSORS_ONLN) && ${cmakecmd} --install . ) 2>>${2}.err 1>>${2}.out ; then
			if [ "Y" = "$gtm_verbose" ] ; then cat ${2}.out ; fi
			cd ../../../.. ; \rm -rf ${2}_tmp
		else
			echo "$2 build failed. The build directory ($PWD) has been saved."
			remove_tmpdir=0
			cd ../../..
		fi
	else
		echo "Unable to download $2. Your Internet connection and/or the GitLab servers may have issues. Please try again later."
		remove_tmpdir=0
		cd ../../..
	fi
}

mktmpdir()
{
	case `uname -s` in
		AIX | SunOS) tmpdirname="/tmp/${USER}_$$_${timestamp}"
			( umask 077 ; mkdir $tmpdirname ) ;;
		HP-UX) tmpdirname=`mktemp`
			( umask 077 ; mkdir $tmpdirname ) ;;
		*) tmpdirname=`mktemp -d` ;;
	esac
	echo $tmpdirname
}

# Turn on debugging if set
if [ "Y" = "$ydb_debug" ] ; then set -x ; fi

alldepflag=1
# List of dependencies that ydbinstall.sh needs and ensure they are present.
utillist="awk basename cat cut expr gzip ldconfig chmod cp date dirname grep id head mkdir mktemp rm sha256sum sed sort tar tr uname"
check_if_utils_exist "Program(s) required to run the ydbinstall/ydbinstall.sh script not found:"
if [ -z "$alldepflag" ] ; then err_exit ; fi

# Ensure this is run from the directory in which it resides to avoid inadvertently deleting files
cd `dirname $0`

# Ensure this is not being sourced so that environment variables in this file do not change the shell environment
if [ "ydbinstall" != `basename -s .sh $0` ] ; then
	echo "Please execute ydbinstall/ydbinstall.sh instead of sourcing it"
	return
fi

# Initialization. Create a unique timestamp. We use 1-second granularity time and a parent process id just in case
# two invocations of ydbinstall.sh happen at the exact same second (YDB#855).
timestamp=`date +%Y%m%d%H%M%S`_$$
if [ -z "$USER" ] ; then USER=`id -un` ; fi


# Defaults that can be over-ridden by command line options to follow
# YottaDB prefixed versions:
if [ -z "$ydb_aim" ] ; then ydb_aim="N" ; fi
if [ -n "$ydb_buildtype" ] ; then gtm_buildtype="$ydb_buildtype" ; fi
if [ -z "$ydb_change_removeipc" ] ; then ydb_change_removeipc="yes" ; fi
if [ -z "$ydb_deprecated" ] ; then ydb_deprecated="Y" ; fi
if [ -n "$ydb_dryrun" ] ; then gtm_dryrun="$ydb_dryrun" ; fi
if [ -z "$ydb_encplugin" ] ; then ydb_encplugin="N" ; fi
if [ -n "$ydb_group_restriction" ] ; then gtm_group_restriction="$ydb_group_restriction" ; fi
if [ -n "$ydb_gtm" ] ; then gtm_gtm="$ydb_gtm" ; fi
if [ -z "$ydb_gui" ] ; then ydb_gui="N" ; fi
if [ -n "$ydb_keep_obj" ] ; then gtm_keep_obj="$ydb_keep_obj" ; fi
if [ -n "$ydb_lcase_utils" ] ; then gtm_lcase_utils="$ydb_lcase_utils" ; fi
if [ -z "$ydb_octo" ] ; then ydb_octo="N" ; fi
if [ -n "$ydb_overwrite_existing" ] ; then gtm_overwrite_existing="$ydb_overwrite_existing" ; fi
if [ -z "$ydb_plugins_only" ] ; then ydb_plugins_only="N" ; fi
if [ -z "$ydb_posix" ] ; then ydb_posix="N" ; fi
if [ -n "$ydb_prompt_for_group" ] ; then gtm_prompt_for_group="$ydb_prompt_for_group" ; fi
if [ -z "$ydb_sodium" ] ; then ydb_sodium="N" ; fi
if [ -n "$ydb_verbose" ] ; then gtm_verbose="$ydb_verbose" ; fi
if [ -z "$ydb_utf8" ] ; then ydb_utf8="N" ; fi
if [ -z "$ydb_zlib" ] ; then ydb_zlib="N" ; fi
# GTM prefixed versions (for backwards compatibility)
if [ -z "$gtm_buildtype" ] ; then gtm_buildtype="pro" ; fi
if [ -z "$gtm_dryrun" ] ; then gtm_dryrun="N" ; fi
if [ -z "$gtm_group_restriction" ] ; then gtm_group_restriction="N" ; fi
if [ -z "$gtm_gtm" ] ; then gtm_gtm="N" ; fi
if [ -z "$gtm_keep_obj" ] ; then gtm_keep_obj="N" ; fi
if [ -z "$gtm_lcase_utils" ] ; then gtm_lcase_utils="Y" ; fi
if [ -z "$gtm_linkenv" ] ; then gtm_linkenv="/usr/local/etc" ; fi
if [ -z "$gtm_linkexec" ] ; then gtm_linkexec="/usr/local/bin" ; fi
if [ -z "$gtm_overwrite_existing" ] ; then gtm_overwrite_existing="N" ; fi
if [ -z "$gtm_prompt_for_group" ] ; then gtm_prompt_for_group="N" ; fi
if [ -z "$gtm_verbose" ] ; then gtm_verbose="N" ; fi

# Initializing internal flags
gtm_group_already="N"
ydb_force_install="N"

# Process command line
while [ $# -gt 0 ] ; do
	case "$1" in
		--aim) ydb_aim="Y" ; shift ;;
		--allplugins)  ydb_aim="Y"
			ydb_encplugin="Y"
			ydb_gui="Y"
			ydb_octo="Y"
			ydb_posix="Y"
			ydb_sodium="Y"
			ydb_zlib="Y" ; shift ;;
		--branch*) tmp=`echo $1 | cut -s -d = -f 2-`
			if [ -n "$tmp" ] ; then ydb_branch=$tmp
			else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then ydb_branch=$2 ; shift
			else echo "--branch needs a value" ; err_exit
				fi
			fi
			shift ;;
		--build-type*) tmp=`echo $1 | cut -s -d = -f 2-`
			if [ -n "$tmp" ] ; then gtm_buildtype=$tmp
			else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then gtm_buildtype=$2 ; shift
				else echo "--build-type needs a value" ; err_exit
				fi
			fi ;;
		--copyenv*) tmp=`echo $1 | cut -s -d = -f 2-`
			if [ -n "$tmp" ] ; then gtm_copyenv=$tmp
			else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then gtm_copyenv=$2 ; shift
				else gtm_copyenv="/usr/local/etc"
				fi
			fi
			unset gtm_linkenv
			shift ;;
		--copyexec*) tmp=`echo $1 | cut -s -d = -f 2-`
			if [ -n "$tmp" ] ; then gtm_copyexec=$tmp
			else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then gtm_copyexec=$2 ; shift
				else gtm_copyexec="/usr/local/bin"
				fi
			fi
			unset gtm_linkexec
			shift ;;
		--debug) ydb_debug="Y" ; set -x ; shift ;;
		--distrib*) tmp=`echo $1 | cut -s -d = -f 2-`
			if [ -n "$tmp" ] ; then ydb_distrib=$tmp
			else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then ydb_distrib=$2 ; shift
				else echo "--distrib needs a value" ; err_exit
				fi
			fi
			shift ;;
		--dry-run) gtm_dryrun="Y" ; shift ;;
		--encplugin) ydb_encplugin="Y" ; shift ;;
		--filename) tmp=`echo $1 | cut -s -d = -f 2-`
			if [ -n "$tmp" ] ; then ydb_filename=$tmp
			elif [ 1 -lt "$#" ] ; then ydb_filename=$2 ; shift
				else echo "--filename needs a value" ; err_exit
			fi
			shift ;;
		--force-install) ydb_force_install="Y" ; shift ;;
		--from-source*) tmp=`echo $1 | cut -s -d = -f 2-`
			if [ -n "$tmp" ] ; then ydb_from_source=$tmp
			else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then ydb_from_source=$2 ; shift
				else ydb_from_source="https://gitlab.com/YottaDB/DB/YDB.git"
				fi
			fi
			shift ;;
		--gtm)
			gtm_gtm="Y"
			shift ;;
		--group-restriction) gtm_group_restriction="Y" ; shift ;; # must come before group*
		--group*) tmp=`echo $1 | cut -s -d = -f 2-`
			if [ -n "$tmp" ] ; then gtm_group=$tmp
			else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then gtm_group=$2 ; shift
				else echo "--group needs a value" ; err_exit
				fi
			fi
			shift ;;
		--gui) ydb_gui="Y" ; shift ;;
		--help) help_exit ;;
		--installdir*) tmp=`echo $1 | cut -s -d = -f 2-`
			if [ -n "$tmp" ] ; then ydb_installdir=$tmp
			else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then ydb_installdir=$2 ; shift
				else echo "--installdir needs a value" ; err_exit
				fi
			fi
			shift ;;
		--keep-obj) gtm_keep_obj="Y" ; shift ;;
		--linkenv*) tmp=`echo $1 | cut -s -d = -f 2-`
			if [ -n "$tmp" ] ; then gtm_linkenv=$tmp
			else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then gtm_linkenv=$2 ; shift
				else gtm_linkenv="/usr/local/etc"
				fi
			fi
			unset gtm_copyenv
			shift ;;
		--linkexec*) tmp=`echo $1 | cut -s -d = -f 2-`
			if [ -n "$tmp" ] ; then gtm_linkexec=$tmp
			else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then gtm_linkexec=$2 ; shift
				else gtm_linkexec="/usr/local/bin"
				fi
			fi
			unset gtm_copyexec
			shift ;;
		--nocopyenv) unset gtm_copyenv ; shift ;;
		--nocopyexec) unset gtm_copyexec ; shift ;;
		--nodeprecated) ydb_deprecated="N" ; shift ;;
		--nolinkenv) unset gtm_linkenv ; shift ;;
		--nolinkexec) unset gtm_linkexec ; shift ;;
		--nopkg-config) ydb_pkgconfig="N" ; shift ;;
		--octo*) tmp=`echo $1 | cut -s -d = -f 2-`
			if [ -n "$tmp" ] ; then octo_cmake=$tmp ; fi
			ydb_octo="Y" ;
			# If Octo, force installation of AIM and POSIX plugins to ensure
			# that the latest versions are being installed. This may require
			# the --overwrite-existing flag to be specified in case one of those
			# plugins exists, but Octo does not.
			ydb_aim="Y";
			ydb_posix="Y" ;
			shift ;;
		--overwrite-existing) gtm_overwrite_existing="Y" ; shift ;;
		--plugins-only) ydb_plugins_only="Y" ; shift ;;
		--posix) ydb_posix="Y" ; shift ;;
		--preserveRemoveIPC) ydb_change_removeipc="no" ; shift ;; # must come before group*
		--prompt-for-group) gtm_prompt_for_group="Y" ; shift ;;
		--sodium) ydb_sodium="Y" ; shift ;;
		--ucaseonly-utils) gtm_lcase_utils="N" ; shift ;;
		--user*) tmp=`echo $1 | cut -s -d = -f 2-`
			if [ -n "$tmp" ] ; then gtm_user=$tmp
			else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then gtm_user=$2 ; shift
				else echo "--user needs a value" ; err_exit
				fi
			fi
			shift ;;
		--utf8*)
			# We previously used to support "--utf8=default" or "--utf8 default" or "--utf8=7.1" or "--utf8 7.1" syntax
			# where an ICU version of "default" or "7.1" (for example) needed to be specified. But in r1.40 (after merging
			# GT.M V7.0-000 changes to sr_unix/gtminstall.sh), sr_unix/ydbinstall.sh started supporting "--utf8" with no value.
			# To maintain backward compatibility, we allow for "--utf8=default" or "--utf8 default" syntax but issue an error
			# if "--utf8=7.1" or "--utf8 7.1" syntax is specified.
			tmp=`echo $1 | cut -s -d = -f 2- | tr DEFAULT default`
			if [ -n "$tmp" ] ; then
				if [ "default" != "$tmp" ] ; then
				    echo "Only value permitted with --utf8= is [default]" ; err_exit
				fi
			fi
			if [ 1 -lt $# ] ; then
				tmp=`echo "$2" | grep -c '^--'`
				if [ "$tmp" -eq 0 ] ; then
					tmp=`echo $2 | tr DEFAULT default`; shift
					if [ -n "$tmp" ] ; then
						if [ "default" != "$tmp" ] ; then
							echo "Only value permitted with --utf8 is [default]" ; err_exit
						fi
					fi
				fi
			fi
			ydb_utf8="Y" ;
			shift ;;
		--verbose) gtm_verbose="Y" ; shift ;;
		--zlib) ydb_zlib="Y" ; shift ;;
		-*) echo Unrecognized option "$1" ; err_exit ;;
		*) if [ -n "$ydb_version" ] ; then echo Nothing must follow the YottaDB/GT.M version ; err_exit
			else ydb_version=$1 ; shift ; fi
	esac
done
if [ "Y" = "$gtm_verbose" ] ; then
	echo Processed command line
	dump_info
	wget_flags="-P"
else
	wget_flags="-qP"
fi

# Add dependencies that the rest of the script and configure.gtc (which ydbinstall.sh calls) needs and ensure they are present.
utillist="$utillist groups getconf chown chgrp file install ld ln locale strip touch wc wget"
check_if_utils_exist "Program(s) required to install YottaDB not found:"

# Add dependencies for normal YottaDB operation.
utillist="nm realpath"
arch=`uname -m`
if [ "armv6l" = "$arch" ] || [ "armv7l" = "$arch" ] ; then
	# ARM platform additionally requires cc (in configure.gtc) to use as the system linker.
	utillist="$utillist cc"
fi

# Check for other required dependencies. Note that dependencies YottaDB requires
# separately checked for first, and even if only plugins are to be installed,
# as YottaDB is required for any plugin to work.
check_if_utils_exist "Program(s) required by YottaDB not found:"

ldconfig=$(command -v ldconfig || command -v /sbin/ldconfig)
# Check for libraries required by YottaDB
shliblist="libelf.so"
if [ "Y" = "$ydb_utf8" ] ; then append_to_str shliblist "libicuio.so" ; fi
check_if_shlibs_exist "Shared library/libraries required by YottaDB not found:"

# Check for additional dependencies beyond those for YottaDB.
unset hdrlist shliblist utillist

# YDBAIM
if [ "Y" = $ydb_aim ] ; then
	append_to_str utillist "gcc git cmake make pkg-config"
fi

# Encryption plugin
if [ "Y" = "$ydb_encplugin" ] ; then
	append_to_str utillist "gcc make tcsh"
	append_to_str shliblist "libconfig.so libssl.so"
	append_to_str hdrlist "gcrypt.h gpg-error.h gpgme.h libconfig.h \
		openssl/bio.h openssl/err.h openssl/evp.h openssl/ssl.h"
fi
# GUI
if [ "Y" = $ydb_gui ] ; then
	append_to_str utillist "cmake cp df gcc git grep pkg-config ps rm stat"
fi
# Octo
if [ "Y" = "$ydb_octo" ] ; then
	append_to_str utillist "bison cmake flex gcc git gzip make pkg-config"
	append_to_str shliblist "libconfig.so libreadline.so"
	append_to_str hdrlist "endian.h getopt.h libconfig.h \
		openssl/conf.h openssl/err.h openssl/evp.h openssl/md5.h openssl/ssl.h \
		readline/history.h readline/readline.h sys/syscall.h"
fi
# POSIX plugin; note that all required headers are part of the POSIX standard
if [ "Y" = "$ydb_posix" ] ; then
	append_to_str utillist "cmake gcc git make pkg-config"
fi

# Zlib plugin; note that all required headers are part of the POSIX standard
if [ "Y" = "$ydb_zlib" ] ; then
	append_to_str utillist "gcc"
	append_to_str shliblist "libz.so"
fi
# libsodium plugin
if [ "Y" = "$ydb_sodium" ] ; then
	append_to_str utillist "cmake gcc git make"
	append_to_str hdrlist "sodium.h"
fi
# Check for required header files, shared libraries and utility programs
check_if_utils_exist "Program(s) required to install selected plugins not found:"
check_if_shlibs_exist "Shared library/libraries required by selected plugins not found:"
check_if_hdrs_exist "Header file(s) required by selected plugins not found:"

# Exit with an error if any dependencies are not met
if [ -z "$alldepflag" ] ; then err_exit ; fi

# Check / set userids and groups
if [ -z "$gtm_user" ] ; then gtm_user=$USER
elif [ "$gtm_user" != "`id -un $gtm_user`" ] ; then
	echo $gtm_user is a non-existent user ; err_exit
fi
if [ "root" = $USER ] ; then
	if [ -z "$gtm_group" ] ; then gtm_group=`id -gn`
	elif [ "root" != "$gtm_user" ] && { id -Gn $gtm_user | grep -qsvw $gtm_group ; } ; then
		echo $gtm_user is not a member of $gtm_group ; err_exit
	fi
else
	echo Non-root installations not currently supported
	if [ "N" = "$gtm_dryrun" ] ; then err_exit
	else echo "Continuing because --dry-run selected"
	fi
fi

osfile="/etc/os-release"
if [ ! -f "$osfile" ] ; then
	echo "/etc/os-release does not exist on host; Not installing YottaDB."
	err_exit
fi
osid=`getosid $osfile`
# Only CentOS 7 and RHEL 7 use "cmake3". All the others including RHEL 8, SLED, SLES, Rocky etc. are fine with cmake.
osver=`grep -w VERSION_ID $osfile | tr -d \" | cut -d= -f2`
osmajorver=`echo $osver | cut -d. -f1`
if { [ "centos" = "$osid" ] || [ "rhel" = "$osid" ]; } && [ 7 = "$osmajorver" ] ; then
	cmakecmd="cmake3"
else
	cmakecmd="cmake"
fi

# Get actual ICU version if UTF-8 install was requested with "default" ICU version
if [ "Y" = "$ydb_utf8" ] ; then
	ydb_found_or_requested_icu_version=`icu_version`
	# At this point "ydb_found_or_requested_icu_version" holds the implicitly determined
	# ICU version to use. Set and export the "ydb_icu_version" env var to reflect this value.
	# This is used in various places (e.g. "install_plugins" call below, "cmake" call below).
	ydb_icu_version=$ydb_found_or_requested_icu_version
	export ydb_icu_version
fi

if [ -n "$ydb_from_source" ] ; then
	# If --from-source is selected, clone the git repo, build and invoke the build's ydbinstall
	if [ -n "$ydb_branch" ] ; then
		echo "Building branch $ydb_branch from repo $ydb_from_source"
	else
		echo "Building branch master from repo $ydb_from_source"
	fi
	ydbinstall_tmp=`mktemp -d`
	cd $ydbinstall_tmp
	#mkdir from_source && cd from_source
	if git clone $ydb_from_source ; then
		cd YDB
		mkdir build && cd build
		# Check if --branch was selected. If so, checkout that branch.
		if [ -n "$ydb_branch" ] ; then
			if ! git checkout $ydb_branch ; then
				echo "branch $ydb_branch does not exist. Exiting. Temporary directory $ydbinstall_tmp will not be deleted."
				err_exit
			fi
		fi
		if [ "dbg" = `echo "$gtm_buildtype" | tr '[:upper:]' '[:lower:]'` ] ; then
			# if --buildtype is dbg, tell CMake to make a dbg build
			cmake_command="${cmakecmd} -D CMAKE_BUILD_TYPE=Debug"
		else
			cmake_command="${cmakecmd} -D CMAKE_BUILD_TYPE=RelWithDebInfo"
		fi
		if ! ${cmake_command} -D CMAKE_INSTALL_PREFIX:PATH=$PWD ../ ; then
			echo "CMake failed. Exiting. Temporary directory $ydbinstall_tmp will not be deleted."
			err_exit
		fi
		if ! make -j `grep -c ^processor /proc/cpuinfo` ; then
			echo "Build failed. Exiting. Temporary directory $ydbinstall_tmp will not be deleted."
			err_exit
		fi
		if ! make install ; then
			echo "Make install failed. Exiting. Temporary directory $ydbinstall_tmp will not be deleted."
			err_exit
		fi
	else
		echo "Cloning git repo $ydb_from_source failed. Check that the URL is correct and accessible then try again."
		err_exit
	fi
	# At this point, YottaDB has been built. Next, we invoke the build's ydbinstall with the same options except --from-source and
	# --branch. To do this, we first have to determine the version number to cd into yottadb_r*
	builddir=`ls -d yottadb_r*`
	cd $builddir
	# Now we have to determine the ydbinstall options to pass to ydbinstall. We ignore the following options for the following reasons:
	# --branch and --from-source : already executed by this block of code
	# --build-type : If a dbg build was requested, we've already built a dbg build with cmake
	# --distrib and --filename : conflicts with --branch and --from-source
	# --help : If this option was selected, ydbinstall would have exited already
	# --plugins-only: There is no need to build YottaDB from source just to add plugins to an already installed YottaDB instance.
	install_options=""
	if [ "Y" = "$ydb_aim" ] ; then install_options="${install_options} --aim" ; fi
	if [ -n "$gtm_copyenv" ] ; then install_options="${install_options} --copyenv ${gtm_copyenv}" ; fi
	if [ -n "$gtm_copyexec" ] ; then install_options="${install_options} --copyexec ${gtm_copyexec}" ; fi
	if [ "Y" = "$ydb_debug" ] ; then install_options="${install_options} --debug" ; fi
	if [ "Y" = "$gtm_dryrun" ] ; then install_options="${install_options} --dry-run" ; fi
	if [ "Y" = "$ydb_encplugin" ] ; then install_options="${install_options} --encplugin" ; fi
	if [ "Y" = "$ydb_force_install" ] ; then install_options="${install_options} --force-install" ; fi
	if [ "Y" = "$gtm_group_restriction" ] ; then install_options="${install_options} --group-restriction" ; fi
	if [ "no" = "$ydb_change_removeipc" ] ; then install_options="${install_options} --preserveRemoveIPC" ; fi
	if [ -n "$gtm_group" ] ; then install_options="${install_options} --group ${gtm_group}" ; fi
	if [ "Y" = "$gtm_gtm" ] ; then install_options="${install_options} --gtm" ; fi
	if [ -n "$ydb_installdir" ] ; then install_options="${install_options} --installdir ${ydb_installdir}" ; fi
	if [ "Y" = "$gtm_keep_obj" ] ; then install_options="${install_options} --keep-obj" ; fi
	if [ -n "$gtm_linkenv" ] ; then install_options="${install_options} --linkenv ${gtm_linkenv}" ; fi
	if [ -n "$gtm_linkexec" ] ; then install_options="${install_options} --linkexec ${gtm_linkexec}" ; fi
	if [ "N" = "$ydb_deprecated" ] ; then install_options="${install_options} --nodeprecated" ; fi
	if [ "Y" = "$ydb_gui" ] ; then install_options="${install_options} --gui" ; fi
	if [ "Y" = "$ydb_octo" ] ; then
		if [ -n "$octo_cmake" ] ; then
			install_options="${install_options} --octo ${octo_cmake}"
		else
			install_options="${install_options} --octo"
		fi
	fi
	if [ "Y" = "$gtm_overwrite_existing" ] ; then install_options="${install_options} --overwrite-existing" ; fi
	if [ "Y" = "$ydb_posix" ] ; then install_options="${install_options} --posix" ; fi
	if [ "Y" = "$gtm_prompt_for_group" ] ; then install_options="${install_options} --prompt-for-group" ; fi
	if [ "N" = "$gtm_lcase_utils" ] ; then install_options="${install_options} --ucaseonly-utils" ; fi
	if [ -n "$gtm_user" ] ; then install_options="${install_options} --user ${gtm_user}" ; fi
	# Use "--utf8 default" (instead of just "--utf8") so "--from-source" works with pre-V7.0-000 ydbinstall
	# invocations too (where --utf8 does require a icu version parameter).
	if [ "Y" = "$ydb_utf8" ] ; then install_options="${install_options} --utf8 default" ; fi
	if [ "Y" = "$gtm_verbose" ] ; then install_options="${install_options} --verbose" ; fi
	if [ "Y" = "$ydb_zlib" ] ; then install_options="${install_options} --zlib" ; fi

	# Now that we have the full set of options, run ydbinstall
	if ./ydbinstall ${install_options} ; then
		# Install succeeded. Exit with code 0 (success)
		\rm -r $ydbinstall_tmp
		exit 0
	else
		echo "Install failed. Temporary directory $ydbinstall_tmp will not be deleted."
		err_exit
	fi
fi

# Set environment variables according to machine architecture
gtm_arch=`uname -m | tr -d _`
case $gtm_arch in
	sun*) gtm_arch="sparc" ;;
esac
gtm_hostos=`uname -s | tr '[:upper:]' '[:lower:]'`
case $gtm_hostos in
	gnu/linux) gtm_hostos="linux" ;;
	hp-ux) gtm_hostos="hpux" ;;
	sun*) gtm_hostos="solaris" ;;
esac
gtm_shlib_support="Y"
case ${gtm_hostos}_${gtm_arch} in
	linux_x8664)
		gtm_sf_dirname="GT.M-amd64-Linux"
		gtm_ftp_dirname="linux_x8664"
		ydb_flavor="x8664"
		gtm_install_flavor="x86_64" ;;
	linux_armv6l)
		ydb_flavor="armv6l" ;;
	linux_armv7l)
		ydb_flavor="armv7l" ;;
	linux_aarch64)
		ydb_flavor="aarch64" ;;
	*) echo Architecture `uname -o` on `uname -m` not supported by this script ; err_exit ;;
esac

if [ "Y" = "$ydb_plugins_only" ]; then
	if [ -z "$ydb_installdir" ] ; then
		# If --installdir was not specified, we first look to $ydb_dist for
		# the YottaDB version. Otherwise, we check pkg-config.
		if [ -d "$ydb_dist" ] ; then
			ydb_installdir=$ydb_dist
		else
			ydb_installdir=$(pkg-config --variable=prefix yottadb)
			ydb_dist=$ydb_installdir
		fi
	else
		ydb_dist=$ydb_installdir
	fi
	# Check that YottaDB is actually installed by looking for the presence of a yottadb executable
	if [ ! -e $ydb_installdir/yottadb ] ; then
		echo "YottaDB not found at $ydb_installdir. Exiting" ; err_exit
	fi
	# If YottaDB is installed with UTF-8, we need that to install plugins
	if [ -d "$ydb_installdir/utf8" ] ; then
		ydb_utf8="Y"
		ydb_found_or_requested_icu_version=`icu_version`
		# Now that we determined the icu version, set ydb_icu_version env var as well.
		ydb_icu_version=$ydb_found_or_requested_icu_version
		export ydb_icu_version
	else
		ydb_utf8="N"
	fi
	# else "$ydb_found_or_requested_icu_version" would already be set because the user had specified the --utf8 option
	# Check that the plugins aren't already installed or that --overwrite-existing is selected
	# Since selecting --octo automatically selects --posix and --aim, we continue the install
	# without overwriting if --aim and/or --posix is already installed and --octo is selected.
	nooverwrite=1
	if [ "Y" != "$gtm_overwrite_existing" ] ; then
		msgsuffix="already installed and --overwrite-existing not specified."
		if [ "Y" = $ydb_aim ] && [ -e $ydb_installdir/plugin/o/_ydbaim.so ] ; then
			echo YDBAIM $msgsuffix ; unset nooverwrite
		fi
		if [ "Y" = $ydb_encplugin ] && [ -e $ydb_installdir/plugin/libgtmcrypt.so ] ; then
			echo YDBEncrypt $msgsuffix ; unset nooverwrite
		fi
		if [ "Y" = $ydb_gui ] && [ -e $ydb_installdir/plugin/o/_ydbgui.so ] ; then
			echo YDBGUI $msgsuffix ; unset nooverwrite
		fi
		if [ "Y" = $ydb_octo ] && [ -d $ydb_installdir/plugin/octo ] ; then
			echo "YDBOcto $msgsuffix" ; unset nooverwrite
		fi
		if [ "Y" = $ydb_posix ] && [ -e $ydb_installdir/plugin/libydbposix.so ] ; then
			echo YDBPOSIX $msgsuffix ; unset nooverwrite
		fi
		if [ "Y" = $ydb_sodium ] && [ -e $ydb_installdir/plugin/libsodium.so ] ; then
			echo YDBSodium $msgsuffix ; unset nooverwrite
		fi
		if [ "Y" = $ydb_zlib ] && [ -e $ydb_installdir/plugin/libgtmzlib.so ] ; then
			echo "YDBZlib $msgsuffix" ; unset nooverwrite
		fi
	fi
	if [ "Y" = "$gtm_dryrun" ] ; then exit 0 ; fi
	if [ -z "$nooverwrite" ] ; then
		err_exit
	else
		tmpdir=`mktmpdir`
		ydb_routines="$tmp($ydb_installdir)" ; export ydb_routines
		remove_tmpdir=1 # remove the tmpdir if the plugin installs are successful
		install_plugins
		if [ 0 = "$remove_tmpdir" ] ; then err_exit; fi	# error seen while installing one or more plugins
		\rm -rf $tmpdir	# Now that we know it is safe to remove $tmpdir, do that before returning normal status
		exit 0
	fi
fi

if [ "N" = "$ydb_force_install" ]; then
	# At this point, we know the current machine architecture is supported by YottaDB
	# but not yet sure if the OS and/or version is supported. Since
	# --force-install is not specified, it is okay to do the os-version check now.
	osver_supported=0 # Consider platform unsupported by default
	# Set an impossible major/minor version by default in case we do not descend down known platforms in if/else below.
	osallowmajorver="999"
	osallowminorver="999"
	buildosfile="../build_os_release"
	if [ -f $buildosfile ] ; then
		buildosid=`getosid $buildosfile`
		buildosver=`grep -w VERSION_ID $buildosfile | tr -d \" | cut -d= -f2`
		if [ "${buildosid}" "=" "${osid}" ] && [ "${buildosver}" "=" "${osver}" ] ; then
			# If the YottaDB build was built on this OS version, it is supported
			osallowmajorver="-1"
			osallowminorver="-1"
		fi
	elif [ "x8664" = "${ydb_flavor}" ] ; then
		if [ "ubuntu" = "${osid}" ] ; then
			if [ -z "$ydb_version" ] || [ 1 = `expr "r1.36" "<" "${ydb_version}"` ] ; then
				# Ubuntu 22.04 onwards is considered supported on x86_64 from r1.38 onwards
				osallowmajorver="22"
				osallowminorver="04"
			else
				# Ubuntu 20.04 onwards is considered supported on x86_64 till r1.36
				osallowmajorver="20"
				osallowminorver="04"
			fi
		elif [ "rhel" = "${osid}" ] ; then
			# RHEL 7 onwards is considered supported on x86_64
			osallowmajorver="7"
			osallowminorver="0"
		elif [ "centos" = "${osid}" ] ; then
			# CentOS 8.x is considered supported on x86_64
			osallowmajorver="8"
			osallowminorver="0"
		elif [ "rocky" = "${osid}" ] ; then
			# Rocky Linux 8.x is considered supported on x86_64
			osallowmajorver="8"
			osallowminorver="0"
		elif [ "sle" = "${osid}" ] ; then
			# SLED and SLES 15 onwards is considered supported on x86_64
			osallowmajorver="15"
			osallowminorver="0"
		elif [ "debian" = "${osid}" ] ; then
			# Debian 11 (buster) onwards is considered supported on x86_64.
			osallowmajorver="11"
			osallowminorver="0"
		fi
	elif [ "aarch64" = "${ydb_flavor}" ] ; then
		if [ "debian" = ${osid} ] ; then
			# Debian 11 (buster) onwards is considered supported on AARCH64
			osallowmajorver="11"
			osallowminorver="0"
		fi
	else
		if [ "armv6l" = "${ydb_flavor}" ] ; then
			if [ "debian" = ${osid} ] ; then
				# Debian 11 onwards is considered supported on ARMV7L/ARMV6L
				osallowmajorver="11"
				osallowminorver="0"
			fi
		fi
	fi
	# It is possible there is no minor version (e.g. Raspbian 9) in which case "cut" will not work
	# as -f2 will give us 9 again. So use awk in that case which will give us "" as $2.
	osminorver=`echo $osver | awk -F. '{print $2}'`
	if [ "" = "$osminorver" ] ; then
		# Needed by "expr" (since it does not compare "" vs numbers correctly)
		# in case there is no minor version field (e.g. Raspbian 9 or even Debian 10 buster/sid).
		osminorver="0"
	fi
	# Some distros (particularly Arch) are missing VERSION_ID altogether.
	# Use a default of 0, which will never be greater than the supported version.
	if [ "${osmajorver:-0}" -gt "$osallowmajorver" ] ; then
		osver_supported=1
	elif [ "$osmajorver" "=" "$osallowmajorver" ] && [ "$osminorver" -ge "$osallowminorver" ] ; then
		osver_supported=1
	else
		if [ "999" = "$osallowmajorver" ] ; then
			# Not a supported OS. Print generic message without OS version #.
			osname=`grep -w NAME $osfile | cut -d= -f2 | cut -d'"' -f2`
			echo "YottaDB not supported on $osname for ${ydb_flavor}. Not installing YottaDB."
		else
			# Supported OS but version is too old to support.
			osname=`grep -w NAME $osfile | cut -d= -f2 | cut -d'"' -f2`
			echo "YottaDB supported from $osname $osallowmajorver.$osallowminorver. Current system is $osname $osver. Not installing YottaDB."
		fi
	fi
	if [ 0 = "$osver_supported" ] ; then
		echo "Specify ydbinstall.sh --force-install to force install"
		err_exit
	fi
fi

# YottaDB version is required - first see if ydbinstall and yottadb/mumps are bundled
if [ -z "$ydb_version" ] ; then
	tmp=`dirname $0`
	if { [ -e "$tmp/yottadb" ] || [ -e "$tmp/mumps" ]; } && [ -e "$tmp/_XCMD.m" ] ; then
		ydb_distrib=$tmp
		ydb_dist=$tmp ; export ydb_dist
		if [ ! -x "$ydb_dist/yottadb" ] ; then
			if [ "root" = "$USER" ] ; then
				chmod +x $ydb_dist/yottadb
			else
				echo >&2 "$ydb_dist/yottadb is not executable and script is not run as root"
				err_exit
			fi
		fi
		tmp=`mktmpdir`
		ydb_routines="$tmp($ydb_dist)" ; export ydb_routines
		# shellcheck disable=SC2016
		if ! ydb_version=`$ydb_dist/yottadb -run %XCMD 'write $piece($zyrelease," ",2)' 2>&1`; then
			echo >&2 "$ydb_dist/yottadb -run %XCMD 'write $piece($zyrelease," ",2)' failed with output $ydb_version"
			err_exit
		fi
		\rm -rf $tmp
	fi
fi
if [ "Y" = "$gtm_verbose" ] ; then
	echo Determined architecture, OS and YottaDB/GT.M version ; dump_info
fi

# See if YottaDB version can be determined from meta data
if [ -z "$ydb_distrib" ] ; then
	ydb_distrib="https://gitlab.com/api/v4/projects/7957109/repository/tags"
fi
if [ "Y" = "$gtm_gtm" ] ; then
	ydb_distrib="http://sourceforge.net/projects/fis-gtm"
fi

tmpdir=`mktmpdir`
gtm_tmpdir=$tmpdir
mkdir $gtm_tmpdir/tmp
latest=`echo "$ydb_version" | tr LATES lates`
if [ -z "$ydb_version" ] || [ "latest" = "$latest" ] ; then
	case $ydb_distrib in
		http://sourceforge.net/projects/fis-gtm | https://sourceforge.net/projects/fis-gtm)
			gtm_gtm="Y"
			if [ "Y" = "$gtm_verbose" ] ; then
				echo wget ${ydb_distrib}/files/${gtm_sf_dirname}/latest to determine latest version
				echo Check proxy settings if wget hangs
			fi
			if { wget $wget_flags $gtm_tmpdir ${ydb_distrib}/files/${gtm_sf_dirname}/latest 1>${gtm_tmpdir}/wget_latest.log 2>&1; } ; then
				ydb_version=`cat ${gtm_tmpdir}/latest`
			else echo Unable to determine YottaDB/GT.M version ; err_exit
			fi ;;
		ftp://*)
			if [ "Y" = "$gtm_verbose" ] ; then
				echo wget $gtm_tmpdir ${ydb_distrib}/${gtm_ftp_dirname}/latest to determine latest version
				echo Check proxy settings if wget hangs
			fi
			if { wget $wget_flags $gtm_tmpdir ${ydb_distrib}/${gtm_ftp_dirname}/latest 1>${gtm_tmpdir}/wget_latest.log 2>&1; } ; then
				ydb_version=`cat ${gtm_tmpdir}/latest`
			else echo Unable to determine YottaDB/GT.M version ; err_exit
			fi ;;
		https://gitlab.com/api/*)
			if [ "Y" = "$gtm_verbose" ] ; then
				echo wget ${ydb_distrib} to determine latest version
				echo Check proxy settings if wget hangs
			fi
			if { wget $wget_flags $gtm_tmpdir ${ydb_distrib} 1>${gtm_tmpdir}/wget_latest.log 2>&1; } ; then
				# Find latest mainline YottaDB release by searching for all "tag_name"s and reverse sorting them based on the
				# release number and taking the first line (which is the most recent release). Note that the sorting will take care
				# of the case if a patch release for a prior version is released after the most recent mainline release
				# (e.g. r1.12 as a patch for r1.10 is released after r1.22 is released). Not sorting will cause r1.12
				# (which will show up as the first line since it is the most recent release) to incorrectly show up as latest.
				#
				# Additionally, it is possible that the latest release does not have any tarballs yet (release is about to
				# happen but release notes are being maintained so it is a release under construction). In that case, we do
				# not want to consider that as the latest release. We find that out by grepping for releases with tarballs
				# (hence the ".pro.tgz" grep below) and only pick those releases that contain tarballs (found by "grep -B 1")
				# before reverse sorting and picking the latest version.
				ydb_version=`sed 's/,/\n/g' ${gtm_tmpdir}/tags | grep -E "tag_name|.pro.tgz" | grep -B 1 ".pro.tgz" | grep "tag_name" | sort -r | head -1 | cut -d'"' -f6`
			fi ;;
		*)
			if [ -f ${ydb_distrib}/latest ] ; then
				ydb_version=`cat ${ydb_distrib}/latest`
				if [ "Y" = "$gtm_verbose" ] ; then echo Version is $ydb_version ; fi
			else echo Unable to determine YottaDB/GT.M version ; err_exit
			fi ;;
	esac
fi
if [ -z "$ydb_version" ] ; then
	echo YottaDB/GT.M version to install is required ; err_exit
fi

# Now that "ydb_version" is determined, get YottaDB/GT.M distribution if ydbinstall is not bundled with distribution
# Note that r1.26 onwards "yottadb" executable exists so use it. If not, use "mumps" executable (pre-r1.26).
if [ -f "${ydb_distrib}/yottadb" ] ; then gtm_tmpdir=$ydb_distrib
elif [ -f "${ydb_distrib}/mumps" ] ; then gtm_tmpdir=$ydb_distrib
else
	tmp=`echo $ydb_version | tr -d .-`
	if [ -z "$ydb_filename" ] ; then
		ydb_filename=""
		if [ "Y" = "$gtm_gtm" ] ; then ydb_filename=gtm_${tmp}_${gtm_hostos}_${ydb_flavor}_${gtm_buildtype}.tar.gz ; fi
	fi
	case $ydb_distrib in
		http://sourceforge.net/projects/fis-gtm | https://sourceforge.net/projects/fis-gtm)
			if [ "Y" = "$gtm_verbose" ] ; then
				echo wget ${ydb_distrib}/files/${gtm_sf_dirname}/${ydb_version}/${ydb_filename} to download tarball
				echo Check proxy settings if wget hangs
			fi
			if { ! wget $wget_flags $gtm_tmpdir ${ydb_distrib}/files/${gtm_sf_dirname}/${ydb_version}/${ydb_filename} \
					1>${gtm_tmpdir}/wget_dist.log 2>&1; } ; then
				echo Unable to download GT.M distribution $ydb_filename ; err_exit
			fi ;;
		https://gitlab.com/api/*)
			if [ "Y" = "$gtm_verbose" ] ; then
				echo wget ${ydb_distrib}/${ydb_version} and parse to download tarball
				echo Check proxy settings if wget hangs
			fi
			if wget $wget_flags $gtm_tmpdir ${ydb_distrib}/${ydb_version} 1>${gtm_tmpdir}/wget_dist.log 2>&1 ; then
				# There might be multiple binary tarballs of YottaDB (for various architectures & platforms).
				# If so, choose the one that corresponds to the current host.
				yottadb_download_urls=`sed 's,/uploads/,\n&,g' ${gtm_tmpdir}/${ydb_version} | grep "^/uploads/" | cut -d')' -f1`
				# The variables "arch" and "platform" determine which tarball gets chosen so set them appropriately below.
				# Determine current host's architecture and store it in "arch" variable.
				# Determine current host's OS and store it in "platform" variable.
				# Modify "arch" and "platform" based on certain conditions below.
				arch=`uname -m | tr -d '_'`
				if expr r1.30 \< "${ydb_version}" >/dev/null; then
					# From r1.32 onwards, the tarball naming conventions changed.
					# Below are the tarball names for the r1.32 tarballs.
					#	yottadb_r132_aarch64_debian11_pro.tgz
					#	yottadb_r132_aarch64_ubuntu2004_pro.tgz
					#	yottadb_r132_armv6l_debian11_pro.tgz
					#	yottadb_r132_x8664_debian11.tgz
					#	yottadb_r132_x8664_rhel7_pro.tgz
					#	yottadb_r132_x8664_rhel8_pro.tgz
					#	yottadb_r132_x8664_ubuntu2004_pro.tgz
					# From r1.36 onwards, the below tarball is also added for SUSE Linux
					#	yottadb_r136_x8664_sle15_pro.tgz
					# From r1.38 onwards, Ubuntu 22.04 is only supported (not Ubuntu 20.04).
					# So the below tarball will be seen.
					#	yottadb_r136_x8664_ubuntu2204_pro.tgz
					# And below are the rules for picking a tarball name for a given target system (OS and architecture).
					platform="${osid}"
					case $arch in
					x8664)
						case "${osid}" in
						rhel|centos|rocky)
							# For x86_64 architecture and RHEL OS, we have separate tarballs for RHEL 7 and RHEL 8.
							# Hence the use of "osmajorver" below in the "platform" variable.
							platform="rhel${osmajorver}"
							# For centos, use the rhel tarball if one exists for the same version.
							#	i.e. CentOS 7 should use the RHEL 7 tarball etc.
							#	i.e. CentOS 8 should use the RHEL 8 tarball etc.
							# Hence the "rhel|centos" usage in the above "case" block.
							;;
						sle)
							# Just like RHEL OS, we have SLE tarballs for both SLES/SLED/OpenSUSE Leap based on the
							# major version. Hence the use of "osmajorver" below in the "platform" variable.
							platform="${osid}${osmajorver}"
							;;
						esac
						;;
					armv7l)
						# armv7l architecture should use the armv6l tarball if available.
						arch="armv6l"
						;;
					esac
				else
					# For r1.30 and older YottaDB releases, use the below logic
					# Below are the tarball names for the r1.30 tarballs.
					#	yottadb_r130_linux_aarch64_pro.tgz
					#	yottadb_r130_linux_armv6l_pro.tgz
					#	yottadb_r130_linux_armv7l_pro.tgz
					#	yottadb_r130_centos8_x8664_pro.tgz
					#	yottadb_r130_debian10_x8664_pro.tgz
					#	yottadb_r130_linux_x8664_pro.tgz
					#	yottadb_r130_rhel7_x8664_pro.tgz
					#	yottadb_r130_ubuntu2004_x8664_pro.tgz
					platform=`uname -s | tr '[:upper:]' '[:lower:]'`
					if [ $arch = "x8664" ] ; then
						# If the current architecture is x86_64 and the distribution is RHEL (including CentOS and SLES)
						# or Debian then set the platform to rhel or debian (not linux) as there are specific tarballs
						# for these distributions. If the distribution is Ubuntu, then set the platform to ubuntu (not linux)
						# if the version is 20.04 or later as there is a specific tarball for newer versions of Ubuntu.
						#
						# To get the correct binary for CentOS, RHEL and SLES, we treat OS major version 7 as rhel and later versions as centos
						case "${osid}" in
						rhel|centos|sle|rocky)
							# CentOS-specific releases of YottaDB for x86_64 happened only after r1.26
							if expr r1.26 \< "${ydb_version}" >/dev/null; then
								# If the OS major version is later than 7, treat it as centos. Otherwise, treat it as rhel.
								if [ 1 = `expr "$osmajorver" ">" "7"` ] ; then
									platform="centos"
								else
									platform="rhel"
								fi
							# RHEL-specific releases of YottaDB for x86_64 happened only starting r1.10 so do this
							# only if the requested version is not r1.00 (the only YottaDB release prior to r1.10)
							elif [ "r1.00" != ${ydb_version} ]; then
								platform="rhel"
							fi
							;;
						debian)
							# Debian-specific releases of YottaDB for x86_64 happened only after r1.24
							if expr r1.24 \< "${ydb_version}" >/dev/null; then
								platform="debian"
							fi
							;;
						ubuntu)
							# Starting with r1.30, there is an Ubuntu 20.04 build where the platform is ubuntu (not linux)
							# so set the platform to ubuntu only if the requested version is r1.30 or later and the
							# Ubuntu version is 20.04 or later.
							if expr r1.28 \< "${ydb_version}" >/dev/null; then
								# If the OS major version is 20 or later, treat it as ubuntu. Otherwise, treat it as linux.
								if [ "${osmajorver:-0}" -gt 19 ] ; then
									platform="ubuntu"
								fi
							fi
							;;
						esac
					fi
				fi
				# Note that as long as we find a tarball with "$arch" and "$platform" in the name, we pick that tarball even if it has
				# a specific version in it and the target system has a lesser version of the OS installed. This is because that case is
				# possible only if --force-install is specified (or else a previous block of code would have done "osallowmajorver" and
				# "osallowminorver" checks and issued appropriate errors). And in that case, we assume the user knows what they are doing
				# and proceed with the install.
				yottadb_download_url=""
				for fullfilename in $yottadb_download_urls
				do
					ydb_filename=$(basename "$fullfilename")	# Get filename without the leading path
					extension="${ydb_filename##*.}"			# Get file extension
					if [ $extension != "tgz" ] ; then
						# Binary tarballs have a ".tgz" extension. Skip anything else.
						continue
					fi
					case $ydb_filename in
						*"$arch"*) ;;			# If tarball has current architecture in its name, consider it
						*) continue ;;			# else move on to next tarball
					esac
					case $ydb_filename in
						*"$platform"*) ;;		# If tarball has current platform in its name, consider it
						*) continue ;;			# else move on to next tarball
					esac
					yottadb_download_url="https://download.yottadb.com/yottadb${fullfilename}"
					break					# Now that we found one tarball, stop looking at other choices
				done
				if [ "$yottadb_download_url" = "" ]; then echo Unable to find YottaDB tarball for ${ydb_version} $platform $arch ; err_exit; fi
				wget $wget_flags $gtm_tmpdir $yottadb_download_url
				if [ ! -f ${gtm_tmpdir}/${ydb_filename} ]; then echo Unable to download YottaDB distribution $ydb_filename ; err_exit; fi
			else echo Error during wget of YottaDB distribution file ${ydb_distrib}/${ydb_filename} ; err_exit
			fi ;;
		ftp://*)
			if [ "Y" = "$gtm_verbose" ] ; then
				echo wget ${ydb_distrib}/${gtm_ftp_dirname}/${tmp}/${ydb_filename} to download tarball
				echo Check proxy settings if wget hangs
			fi
			if { ! wget $wget_flags $gtm_tmpdir ${ydb_distrib}/${gtm_ftp_dirname}/${tmp}/${ydb_filename} \
					1>${gtm_tmpdir}/wget_dist.log 2>&1 ; } ; then
				echo Unable to download GT.M distribution $ydb_filename ; err_exit
			fi ;;
		*)
			if [ -f ${ydb_distrib}/${ydb_filename} ] ; then
				if [ "Y" = "$gtm_verbose" ] ; then echo tarball is ${ydb_distrib}/${ydb_filename} ; fi
				ln -s ${ydb_distrib}/${ydb_filename} $gtm_tmpdir
			else echo Unable to locate YottaDB/GT.M distribution file ${ydb_distrib}/${ydb_filename} ; err_exit
			fi ;;
	esac
	( cd $gtm_tmpdir/tmp ; gzip -d < ${gtm_tmpdir}/${ydb_filename} | tar xf - 1>${gtm_tmpdir}/tar.log 2>&1 )
fi
if [ "Y" = "$gtm_verbose" ] ; then echo Downloaded and unpacked YottaDB/GT.M distribution ; dump_info ; fi

# Check installation settings & provide defaults as needed
if [ -z "$ydb_installdir" ] ; then
	if [ "N" = "$gtm_gtm" ] ; then
		ydbver=`echo $ydb_version | tr '[:upper:]' '[:lower:]' | tr -d '.-'`
		ydb_installdir=/usr/local/lib/yottadb/${ydbver}
	else ydb_installdir=/usr/local/lib/fis-gtm/${ydb_version}_${gtm_install_flavor}
	fi
fi
# if install directory is relative then need to make it absolute before passing it to configure
# (or else configure will create a subdirectory under $tmpdir (/tmp/.*) and install YottaDB there which is not what we want)
if [ `echo $ydb_installdir | grep -c '^/'` -eq 0 ] ; then
	ydb_installdir=`pwd`/$ydb_installdir
fi
if [ -d "$ydb_installdir" ] && [ "Y" != "$gtm_overwrite_existing" ] ; then
	echo $ydb_installdir exists and --overwrite-existing not specified ; err_exit
fi

if [ "Y" = "$gtm_verbose" ] ; then echo Finished checking options and assigning defaults ; dump_info ; fi

# Prepare input to YottaDB configure script. The corresponding questions in configure.gtc are listed below in comments
if [ "root" = $(id -un) ] ; then gtm_configure_in=${gtm_tmpdir}/configure_${timestamp}.in
else gtm_configure_in=${tmpdir}/configure_${timestamp}.in
fi
export ydb_change_removeipc			# Signal configure.gtc to set RemoveIPC=no or not, if needed
issystemd=`command -v systemctl`
if [ "N" = "$gtm_dryrun" ] && [ "" != "$issystemd" ] ; then
	# It is a systemd installation
	# Check if RemoveIPC=no is set. If not, set it if user hasn't specified --preserveRemoveIPC
	logindconf="/etc/systemd/logind.conf"
	removeipcopt=`awk -F = '/^RemoveIPC/ {opt=$2} END{print opt}' $logindconf`
	if [ "no" != "$removeipcopt" ] ; then
		if [ "yes" != "$ydb_change_removeipc" ] ; then
			# shellcheck disable=SC2016
			echo 'YottaDB needs to have setting of `RemoveIPC=no` in `/etc/systemd/logind.conf`'
			echo 'in order to function correctly. After placing it there, you will need to restart'
			echo 'the systemd-logind service using this command "systemctl restart systemd-logind".'
			echo ''
			echo 'If your Desktop session is managed by the systemd-logind service (e.g., a Gnome'
			echo 'or Plasma desktop), restarting systemd-logind will restart your GUI. Save all'
			echo 'your work prior to issuing "systemctl restart systemd-logind".'
		else
			echo 'Since you are running systemd we changed memory settings and you need to restart'
			echo 'the systemd-logind service using this command "systemctl restart systemd-logind".'
			echo ''
			echo 'If your Desktop session is managed by the systemd-logind service (e.g., a Gnome'
			echo 'or Plasma desktop), restarting systemd-logind will restart your GUI. Save all'
			echo 'your work prior to issuing "systemctl restart systemd-logind".'
		fi
	fi
fi

{
	echo $gtm_user		# Response to : "What user account should own the files?"
	echo $gtm_group		# Response to : "What group should own the files?"
	echo $gtm_group_restriction	# Response to : "Should execution of YottaDB be restricted to this group?"
	echo $ydb_installdir	# Response to : "In what directory should YottaDB be installed?"
	echo y			# Response to one of two possible questions
				#	"Directory $ydb_dist exists. If you proceed with this installation then some files will be over-written. Is it ok to proceed?"
				#	"Directory $ydb_dist does not exist. Do you wish to create it as part of this installation? (y or n)"
	if [ "Y" != "$ydb_utf8" ] ; then echo n		# Response to : "Should UTF-8 support be installed?"
	else echo y					# Response to : "Should UTF-8 support be installed?"
	fi
	if [ "Y" = $ydb_deprecated ] ; then echo y	# Response to : "Should deprecated components be installed?"
	else echo n					# Response to : "Should deprecated components be installed?"
	fi
	echo $gtm_lcase_utils	# Response to : "Do you want uppercase and lowercase versions of the MUMPS routines?"
	if [ "Y" = $gtm_shlib_support ] ; then echo $gtm_keep_obj ; fi	# Response to : "Object files of M routines placed in shared library $ydb_dist/libyottadbutil$ext. Keep original .o object files (y or n)?"
	echo n			# Response to : "Installation completed. Would you like all the temporary files removed from this directory?"
} >> $gtm_configure_in
if [ "Y" = "$gtm_verbose" ] ; then echo Prepared configuration file ; cat $gtm_configure_in ; dump_info ; fi

# Run the YottaDB configure script
if [ "$ydb_distrib" != "$gtm_tmpdir" ] ; then
	chmod +w $gtm_tmpdir/tmp
	cd $gtm_tmpdir/tmp
	# Starting YottaDB r1.10, unpacking the binary tarball creates an additional directory (e.g. yottadb_r122)
	# before the untar so cd into that subdirectory to get at the "configure" script from the distribution.
	if [ "N" = "$gtm_gtm" ] && [ "r1.00" != ${ydb_version} ] ; then
		cd yottadb_r*
	fi
fi

# Stop here if this is a dry run
if [ "Y" = "$gtm_dryrun" ] ; then echo "Terminating without making any changes as --dry-run specified" ; exit ; fi

if [ -e configure.sh ] ; then \rm -f configure.sh ; fi

tmp=`head -1 configure | cut -f 1`
if [ "#!/bin/sh" != "$tmp" ] ; then
	echo "#!/bin/sh" >configure.sh
fi
cat configure >>configure.sh
chmod +x configure.sh

if ! sh -x ./configure.sh <$gtm_configure_in 1> $gtm_tmpdir/configure_${timestamp}.out 2>$gtm_tmpdir/configure_${timestamp}.err; then
	echo "configure.sh failed. Output follows"
	cat $gtm_tmpdir/configure_${timestamp}.out $gtm_tmpdir/configure_${timestamp}.err
	err_exit
fi

if [ "Y" = "$gtm_gtm" ] ; then
	product_name="GT.M"
else
	product_name="YottaDB"
	# Add ydbinstall to the installation if the installation was a YottaDB installation
	cp ydbinstall $ydb_installdir
fi
\rm -rf ${tmpdir:?}/*	# Now that install is successful, remove everything under temporary directory
			# We might still need this temporary directory for installing optional plugins (encplugin, posix etc.)
			# if they have been specified in the ydbinstall.sh command line. Not having a valid current directory
			# will cause YDB-E-SYSCALL errors from "getcwd()" in ydb_env_set calls made later.
echo $product_name version $ydb_version installed successfully at $ydb_installdir

# Create copies of, or links to, environment scripts and ydb & gtm executables
if [ -n "$gtm_linkenv" ] ; then
	dirensure $gtm_linkenv
	( cd $gtm_linkenv ; \rm -f ydb_env_set ydb_env_unset gtmprofile ; ln -s $ydb_installdir/ydb_env_set $ydb_installdir/ydb_env_unset $ydb_installdir/gtmprofile ./ )
	if [ "Y" = "$gtm_verbose" ] ; then echo Linked env ; ls -l $gtm_linkenv ; fi
elif [ -n "$gtm_copyenv" ] ; then
	dirensure $gtm_copyenv
	( cd $gtm_copyenv ; \rm -f ydb_env_set ydb_env_unset gtmprofile ; cp -P $ydb_installdir/ydb_env_set $ydb_installdir/ydb_env_unset $ydb_installdir/gtmprofile ./ )
	if [ "Y" = "$gtm_verbose" ] ; then echo Copied env ; ls -l $gtm_copyenv ; fi
fi
if [ -n "$gtm_linkexec" ] ; then
	dirensure $gtm_linkexec
	( cd $gtm_linkexec ; \rm -f ydb gtm ; ln -s $ydb_installdir/ydb $ydb_installdir/gtm ./ )
	if [ "Y" = "$gtm_verbose" ] ; then echo Linked exec ; ls -l $gtm_linkexec ; fi
elif [ -n "$gtm_copyexec" ] ; then
	dirensure $gtm_copyexec
	( cd $gtm_copyexec ; \rm -f ydb gtm ; cp -P $ydb_installdir/ydb $ydb_installdir/gtm ./ )
	if [ "Y" = "$gtm_verbose" ] ; then echo Copied exec ; ls -l $gtm_copyexec ; fi
fi

# Create the pkg-config file
if [ "N" != "$ydb_pkgconfig" ] ; then
	pcfilepath=/usr/share/pkgconfig
	cat > ${ydb_installdir}/yottadb.pc << EOF
prefix=${ydb_installdir}

exec_prefix=\${prefix}
includedir=\${prefix}
libdir=\${exec_prefix}

Name: YottaDB
Description: YottaDB database library
Version: ${ydb_version}
Cflags: -I\${includedir}
Libs: -L\${libdir} -lyottadb -Wl,-rpath,\${libdir}
EOF

	# Now place it where the system can find it
	# We strip the "r" and "." to perform a numeric comparision between the versions
	# YottaDB will only ever increment versions, so a larger number indicates a newer version
	if [ ! -f ${pcfilepath}/yottadb.pc ] || {
			existing_version=$(grep "^Version: " ${pcfilepath}/yottadb.pc | cut -s -d " " -f 2)
			! expr "$existing_version" \> "$ydb_version" >/dev/null
			}; then
		if [ ! -d $pcfilepath ]
		then
			mkdir $pcfilepath
		fi
		cp ${ydb_installdir}/yottadb.pc ${pcfilepath}/yottadb.pc
		echo $product_name pkg-config file installed successfully at ${pcfilepath}/yottadb.pc
	else
		echo Skipping $product_name pkg-config file install for ${ydb_version} as newer version $existing_version exists at ${pcfilepath}/yottadb.pc
	fi
fi

# install optional components if they were selected

remove_tmpdir=1	# It is okay to remove $tmpdir at the end assuming ydbinstall is successful at installing plugins.
		# Any errors while installing any plugin will set this variable to 0.

install_plugins

if [ 0 = "$remove_tmpdir" ] ; then err_exit; fi	# error seen while installing one or more plugins

\rm -rf $tmpdir	# Now that we know it is safe to remove $tmpdir, do that before returning normal status
exit 0
