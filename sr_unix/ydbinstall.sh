#!/bin/sh -
#################################################################
# Copyright (c) 2014-2019 Fidelity National Information         #
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
#	under a license.  If you do not know the terms of	#
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
#
# NOTE: This script requires the GNU wget program to download
# distribution files that are not on the local file system.

# Turn on debugging if set
if [ "Y" = "$ydb_debug" ] ; then set -x ; fi

check_if_util_exists()
{
	command -v $1 >/dev/null 2>&1 || command -v /sbin/$1 >/dev/null 2>&1 || { echo >&2 "Utility [$1] is needed by ydbinstall.sh but not found. Exiting."; exit 1; }
}

# Check all utilities that ydbinstall.sh will use and ensure they are present. If not error out at beginning.
utillist="date id grep uname mktemp cut tr dirname chmod rm mkdir cat wget sed sort head basename ln gzip tar xargs sh cp"
# Check all utilities that configure.gtc (which ydbinstall.sh calls) will additionally use and ensure they are present.
# If not error out at beginning instead of erroring out midway during the install.
utillist="$utillist ps file wc touch chown chgrp groups getconf awk expr locale install ld strip"
# Check utilities used by YottaDB for normal operation
utillist="$utillist nm realpath ldconfig"
arch=`uname -m`
if [ "armv6l" = "$arch" ] || [ "armv7l" = "$arch" ] ; then
	# ARM platform requires cc (in configure.gtc) to use as the system linker (ld does not work yet)
	utillist="$utillist cc"
fi
for util in $utillist
do
	check_if_util_exists $util
done

# Ensure this is not being sourced so that environment variables in this file do not change the shell environment
if [ "ydbinstall" != `basename -s .sh $0` ] ; then
    echo "Please execute ydbinstall/ydbinstall.sh instead of sourcing it"
    return
fi

# Ensure this is run from the directory in which it resides to avoid inadvertently deleting files
cd `dirname $0`

# Initialization. Create a unique timestamp. We use 1-second granularity time and a parent process id just in case
# two invocations of ydbinstall.sh happen at the exact same second (YDB#855).
timestamp=`date +%Y%m%d%H%M%S`_$$
if [ -z "$USER" ] ; then USER=`id -un` ; fi

# Functions
dump_info()
{
    set +x
    if [ -n "$icu_version" ] ; then echo icu_version " : " $icu_version ; fi
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

help_exit()
{
    set +x
    echo "ydbinstall [option] ... [version]"
    echo "Options are:"
    echo "--aim                    -> installs AIM plug-in"
    echo "--branch branchname      -> builds YottaDB from a specific git branch; use with --from-source"
    echo "--build-type buildtype   -> type of YottaDB build, default is pro"
    echo "--copyenv [dirname]      -> copy ydb_env_set, ydb_env_unset, and gtmprofile files to dirname, default /usr/local/etc; incompatible with linkenv"
    echo "--copyexec [dirname]     -> copy ydb & gtm scripts to dirname, default /usr/local/bin; incompatible with linkexec"
    echo "--debug                  -> turn on debugging with set -x"
    echo "--distrib dirname or URL -> source directory for YottaDB/GT.M distribution tarball, local or remote"
    echo "--dry-run                -> do everything short of installing YottaDB, including downloading the distribution"
    echo "--encplugin              -> compile and install the encryption plugin"
    echo "--filename filename      -> name of YottaDB distribution tarball"
    echo "--force-install          -> install even if the current platform is not supported"
    echo "--from-source repo       -> builds and installs YottaDB from a git repo; defaults to building the latest master from gitlab if not specified; check README for list of prerequisites to build from source"
    echo "--group group            -> group that should own the YottaDB installation"
    echo "--group-restriction      -> limit execution to a group; defaults to unlimited if not specified"
    echo "--gtm                    -> install GT.M instead of YottaDB"
    echo "--gui                    -> download and install the YottaDB GUI"
    echo "--help                   -> print this usage information"
    echo "--installdir dirname     -> directory where YottaDB is to be installed; defaults to /usr/local/lib/yottadb/version"
    echo "--keep-obj               -> keep .o files of M routines (normally deleted on platforms with YottaDB support for routines in shared libraries)"
    echo "--linkenv [dirname]      -> create link in dirname to ydb_env_set, ydb_env_unset & gtmprofile files, default /usr/local/etc; incompatible with copyenv"
    echo "--linkexec [dirname]     -> create link in dirname to ydb & gtm scripts, default /usr/local/bin; incompatible with copyexec"
    echo "--nocopyenv              -> do not copy ydb_env_set, ydb_env_unset, and gtmprofile to another directory"
    echo "--nocopyexec             -> do not copy ydb & gtm scripts to another directory"
    echo "--nodeprecated           -> do not install deprecated components, specifically %DSEWRAP"
    echo "--nolinkenv              -> do not create link to ydb_env_set, ydb_env_unset, and gtmprofile from another directory"
    echo "--nolinkexec             -> do not create link to ydb & gtm scripts from another directory"
    echo "--nopkg-config           -> do not create yottadb.pc for pkg-config, or update an existing file"
    echo "--octo parameters        -> download and install Octo; also installs required POSIX and AIM plugins. Specify optional cmake parameters for Octo as necessary"
    echo "--overwrite-existing     -> install into an existing directory, overwriting contents; defaults to requiring new directory"
    echo "--plugins-only           -> just install plugins for an existing YottaDB installation, not YottaDB"
    echo "--posix                  -> download and install the POSIX plugin"
    echo "--preserveRemoveIPC      -> do not allow changes to RemoveIPC in /etc/systemd/login.conf if needed; defaults to allow changes"
    echo "--prompt-for-group       -> YottaDB installation script will prompt for group; default is yes for production releases V5.4-002 or later, no for all others"
    echo "--ucaseonly-utils        -> install only upper case utility program names; defaults to both if not specified"
    echo "--user username          -> user who should own YottaDB installation; default is root"
    echo "--utf8 ICU_version       -> install UTF-8 support using specified  major.minor ICU version; specify default to use version provided by OS as default"
    echo "--verbose                -> output diagnostic information as the script executes; default is to run quietly"
    echo "--zlib                   -> download and install the zlib plugin"
    echo "Options that take a value (e.g, --group) can be specified as either --option=value or --option value."
    echo "Options marked with \"*\" are likely to be of interest primarily to YottaDB developers."
    echo "Version is defaulted from yottadb file if one exists in the same directory as the installer."
    echo "This version must run as root."
    echo ""
    echo "Example usages are (assumes latest YottaDB release is r1.38 and latest GT.M version is V7.0-005)"
    echo "  $0                          # installs latest YottaDB release (r1.38) at /usr/local/lib/yottadb/r138"
    echo "  $0 --utf8 default           # installs YottaDB release r1.38 with added support for UTF-8"
    echo "  $0 --installdir /r138 r1.38 # installs YottaDB r1.38 at /r138"
    echo "  $0 --gtm                    # installs latest GT.M version (V7.0-005) at /usr/local/lib/fis-gtm/V7.0-005_x86_64"
    echo ""
    echo "As options are processed left to right, later options can override earlier options."
    echo ""
    exit 1
}

# This function ensures that a target directory exists
dirensure()
{
    if [ ! -d "$1" ] ; then
	mkdir -p "$1"
	if [ "Y" = "$gtm_verbose" ] ; then echo Directory "$1" created ; fi
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
# If file name is "libicuio.so.70", the below will return "70".
# If file name is "libicuio.so.suse65.1", the below will return "65.1.suse" (needed for YottaDB to work on SLED 15).
# There is a M version of this function in sr_unix/ydbenv.mpt
# It needs to be maintained in parallel to this function
icu_version()
{
	$ldconfig -p | grep -m1 -F libicuio.so. | cut -d" " -f1 | sed 's/.*libicuio.so.\([a-z]*\)\([0-9\.]*\)/\2.\1/;s/\.$//;'
}

# This function installs the selected plugins. Before calling it, $ydb_installdir and $tmpdir need to be set so that it can
# find the right place to install the plugins and the right place to build the plugins respectively. This function will
# set remove_tmpdir to 0 if one or more plugin builds fail.
install_plugins()
{
	if [ "Y" = $ydb_posix ] ; then
		echo "Now installing YDBPosix"
		cd $tmpdir	# Get back to top level temporary directory as the current directory
		mkdir posix_tmp
		cd posix_tmp
		export ydb_dist=${ydb_installdir}
		if curl -fSsLO https://gitlab.com/YottaDB/Util/YDBPosix/-/archive/master/YDBPosix-master.tar.gz; then
			tar xzf YDBPosix-master.tar.gz
			cd YDBPosix-master
			mkdir build && cd build
			${cmakecmd} ../
			if make -j `grep -c ^processor /proc/cpuinfo` && make install; then
				# Save the build directory if either of the make commands return a non-zero exit code. Otherwise, remove it.
				cd ../../..
				rm -R posix_tmp
			else
				echo "YDBPosix build failed. The build directory ($PWD) has been saved."
				remove_tmpdir=0
			fi
		else
			echo "Unable to download YDBPosix. Your internet connection and/or the gitlab servers may be down. Please try again later."
			remove_tmpdir=0
		fi
	fi

	if [ "Y" = $ydb_aim ] ; then
		echo "Now installing YDBAIM"
		cd $tmpdir
		mkdir aim_tmp
		cd aim_tmp
		export ydb_dist=${ydb_installdir}
		if curl -fSsLO https://gitlab.com/YottaDB/Util/YDBAIM/-/archive/master/YDBAIM-master.tar.gz; then
			tar xzf YDBAIM-master.tar.gz
			cd YDBAIM-master
			mkdir build && cd build
			${cmakecmd} ../
			if make -j `grep -c ^processor /proc/cpuinfo` && make install; then
				# Save the build directory if either of the make commands return a non-zero exit code. Otherwise, remove it.
				cd ../../..
				rm -R aim_tmp
			else
				echo "YDBAIM build failed. The build directory ($PWD) has been saved."
				remove_tmpdir=0
			fi
		else
			echo "Unable to download YDBAIM. Your internet connection and/or the gitlab servers may be down. Please try again later"
			remove_tmpdir=0
		fi
	fi

	if [ "Y" = $ydb_encplugin ] ; then
		echo "Now installing YDBEncrypt"
		cd $tmpdir	# Get back to top level temporary directory as the current directory
		mkdir enc_tmp && cd enc_tmp
		url="https://gitlab.com/YottaDB/Util/YDBEncrypt.git"
		if git clone -q ${url} .; then
			ydb_dist=${ydb_installdir} make -j `grep -c ^processor /proc/cpuinfo`
			if ydb_dist=${ydb_installdir} make install; then
				# Save the build directory if the make install command returns a non-zero exit code. Otherwise, remove it.
				cd ..
				rm -R enc_tmp
			else
				echo "YDBEncrypt build failed. The build directory ($PWD/enc_tmp) has been saved."
				remove_tmpdir=0
			fi
		else
			echo "Unable to download YDBEncrypt. Your internet connection and/or the gitlab servers may be down. Please try again later."
			remove_tmpdir=0
		fi
		# rename gtmcrypt to ydbcrypt and create a symbolic link for backward compatibility
		mv ${ydb_installdir}/plugin/gtmcrypt ${ydb_installdir}/plugin/ydbcrypt
		ln -s ${ydb_installdir}/plugin/ydbcrypt ${ydb_installdir}/plugin/gtmcrypt
		# Enable execute permissions on .sh scripts in the YDBEncrypt plugin
		chmod +x ${ydb_installdir}/plugin/ydbcrypt/*.sh
	fi

	if [ "Y" = $ydb_zlib ] ; then
		echo "Now installing YDBZlib"
		cd $tmpdir	# Get back to top level temporary directory as the current directory
		mkdir zlib_tmp
		cd zlib_tmp
		if curl -fSsLO https://gitlab.com/YottaDB/Util/YDBZlib/-/archive/master/YDBZlib-master.tar.gz; then
			tar xzf YDBZlib-master.tar.gz
			cd YDBZlib-master
			if gcc -c -fPIC -I${ydb_installdir} gtmzlib.c && gcc -o libgtmzlib.so -shared gtmzlib.o; then
				# Save the build directory if either of the gcc commands return a non-zero exit code. Otherwise, remove it.
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
				rm -R zlib_tmp
			else
				echo "YDBZlib build failed. The build directory ($PWD/zlib_tmp) has been saved."
				remove_tmpdir=0
			fi
		else
			echo "Unable to download YDBZlib. Your internet connection and/or the gitlab servers may be down. Please try again later."
			remove_tmpdir=0
		fi
	fi

	if [ "Y" = $ydb_gui ] ; then
		echo "Now installing YDBGUI"
		cd $tmpdir	# Get back to top level temporary directory as the current directory
		export ydb_dist=${ydb_installdir}
		if git clone https://gitlab.com/YottaDB/UI/YDBGUI.git YDBGUI-master; then
			cd YDBGUI-master
			mkdir build && cd build
			if cmake .. && make && make install; then
				cd ../..
				rm -R YDBGUI-master
			else
				echo "YDBGUI build failed. The build directory ($PWD/YDBGUI-master) has been savd."
				remove_tmpdir=0
			fi
		else
		    echo "Unable to download YDBGUI. Your internet connection and/or the gitlab servers may be down. Please try again later."
		    remove_tmpdir=0
		fi
	fi

	if [ "Y" = $ydb_octo ] ; then
		echo "Now installing YDBOcto"
		cd $tmpdir	# Get back to top level temporary directory as the current directory
		export ydb_dist=${ydb_installdir}
		if git clone https://gitlab.com/YottaDB/DBMS/YDBOcto.git YDBOcto-master; then
			cd YDBOcto-master
			mkdir build
			cd build
			${cmakecmd} ${octo_cmake} ../
			if make -j `grep -c ^processor /proc/cpuinfo` && make install; then
				# Save the build directory if either of the make commands return a non-zero exit code. Otherwise, remove it.
				cd ../..
				rm -R YDBOcto-master
			else
				echo "YDBOcto build failed. The build directory ($PWD/YDBOcto-master) and the tarball ($PWD/YDBOcto-master.tar.gz) have been saved."
				remove_tmpdir=0
			fi
		else
			echo "Unable to download YDBOcto. Your internet connection and/or the gitlab servers may be down. Please try again later."
			remove_tmpdir=0
		fi
	fi
}

# Defaults that can be over-ridden by command line options to follow
# YottaDB prefixed versions:
if [ -n "$ydb_buildtype" ] ; then gtm_buildtype="$ydb_buildtype" ; fi
if [ -n "$ydb_keep_obj" ] ; then gtm_keep_obj="$ydb_keep_obj" ; fi
if [ -n "$ydb_dryrun" ] ; then gtm_dryrun="$ydb_dryrun" ; fi
if [ -n "$ydb_group_restriction" ] ; then gtm_group_restriction="$ydb_group_restriction" ; fi
if [ -n "$ydb_gtm" ] ; then gtm_gtm="$ydb_gtm" ; fi
if [ -z "$ydb_gui" ] ; then ydb_gui="N" ; fi
if [ -n "$ydb_lcase_utils" ] ; then gtm_lcase_utils="$ydb_lcase_utils" ; fi
if [ -n "$ydb_overwrite_existing" ] ; then gtm_overwrite_existing="$ydb_overwrite_existing" ; fi
if [ -n "$ydb_prompt_for_group" ] ; then gtm_prompt_for_group="$ydb_prompt_for_group" ; fi
if [ -n "$ydb_verbose" ] ; then gtm_verbose="$ydb_verbose" ; fi
if [ -z "$ydb_change_removeipc" ] ; then ydb_change_removeipc="yes" ; fi
if [ -z "$ydb_deprecated" ] ; then ydb_deprecated="Y" ; fi
if [ -z "$ydb_encplugin" ] ; then ydb_encplugin="N" ; fi
if [ -z "$ydb_posix" ] ; then ydb_posix="N" ; fi
if [ -z "$ydb_aim" ] ; then ydb_aim="N" ; fi
if [ -z "$ydb_octo" ] ; then ydb_octo="N" ; fi
if [ -z "$ydb_zlib" ] ; then ydb_zlib="N" ; fi
if [ -z "$ydb_utf8" ] ; then ydb_utf8="N" ; fi
if [ -z "$ydb_plugins_only" ] ; then ydb_plugins_only="N" ; fi
# GTM prefixed versions (for backwards compatibility)
if [ -z "$gtm_buildtype" ] ; then gtm_buildtype="pro" ; fi
if [ -z "$gtm_keep_obj" ] ; then gtm_keep_obj="N" ; fi
if [ -z "$gtm_dryrun" ] ; then gtm_dryrun="N" ; fi
if [ -z "$gtm_group_restriction" ] ; then gtm_group_restriction="N" ; fi
if [ -z "$gtm_gtm" ] ; then gtm_gtm="N" ; fi
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
	    ydb_posix="Y" ;
	    ydb_aim="Y";
	    shift ;;
        --overwrite-existing) gtm_overwrite_existing="Y" ; shift ;;
	--plugins-only) ydb_plugins_only="Y" ; shift ;;
	--posix) ydb_posix="Y" ; shift ;;
	--aim) ydb_aim="Y" ; shift ;;
        --preserveRemoveIPC) ydb_change_removeipc="no" ; shift ;; # must come before group*
        --prompt-for-group) gtm_prompt_for_group="Y" ; shift ;;
        --ucaseonly-utils) gtm_lcase_utils="N" ; shift ;;
        --user*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then gtm_user=$tmp
            else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then gtm_user=$2 ; shift
                else echo "--user needs a value" ; err_exit
                fi
            fi
            shift ;;
        --utf8*) tmp=`echo $1 | cut -s -d = -f 2- | tr DEFAULT default`
            if [ -n "$tmp" ] ; then icu_version=$tmp
            else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then icu_version=`echo $2 | tr DEFAULT default`; shift
                else echo "--utf8 needs a value" ; err_exit
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
if [ "Y" = "$gtm_verbose" ] ; then echo Processed command line ; dump_info ; fi

# Check / set userids and groups
if [ -z "$gtm_user" ] ; then gtm_user=$USER
elif [ "$gtm_user" != "`id -un $gtm_user`" ] ; then
    echo $gtm_user is a non-existent user ; err_exit
fi
if [ "root" = $USER ] ; then
    if [ -z "$gtm_group" ] ; then gtm_group=`id -gn`
    elif [ "root" != "$gtm_user" ] && [ "$gtm_group" != "`id -Gn $gtm_user | xargs -n 1 | grep $gtm_group`" ] ; then
        echo $gtm_user is not a member of $gtm_group ; err_exit
    fi
else
    echo Non-root installations not currently supported
    if [ "N" = "$gtm_dryrun" ] ; then err_exit
    else echo "Continuing because --dry-run selected"
    fi
fi

# Check whether libelf.so exists; issue an error and exit if it does not
ldconfig=$(command -v ldconfig || command -v /sbin/ldconfig)
$ldconfig -p | grep -qs /libelf.so ; ydb_tmp_stat=$?
if [ 0 -ne $ydb_tmp_stat ] ; then
	echo >&2 "Library libelf.so is needed by YottaDB but not found. Exiting." ; exit $ydb_tmp_stat
fi

# If UTF-8 support is requested, but libicuio is not found, issue an error and exit
if [ "Y" = "$ydb_utf8" ] ; then
   $ldconfig -p | grep -qs /libicuio ; ydb_tmp_stat=$?
   if [ 0 -ne $ydb_tmp_stat ] ; then
       echo >&2 "UTF-8 support requested but libicuio.so not found. Exiting." ; exit $ydb_tmp_stat
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
	if [ "default" = $icu_version ] ; then
		ydb_found_or_requested_icu_version=`icu_version`
	else
		ydb_found_or_requested_icu_version=$icu_version
	fi
	# At this point "ydb_found_or_requested_icu_version" holds the user specified or implicitly determined
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
	if [ "Y" = "$ydb_utf8" ] ; then install_options="${install_options} --utf8 ${icu_version}" ; fi
	if [ "Y" = "$gtm_verbose" ] ; then install_options="${install_options} --verbose" ; fi
	if [ "Y" = "$ydb_zlib" ] ; then install_options="${install_options} --zlib" ; fi

	# Now that we have the full set of options, run ydbinstall
	if ./ydbinstall ${install_options} ; then
		# Install succeeded. Exit with code 0 (success)
		rm -r $ydbinstall_tmp
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
	# Check if UTF8 is installed.
	if [ "Y" != "$ydb_utf8" ] ; then
		# ydb_found_or_requested_icu_version has NOT already been determined.
		if [ -d "$ydb_installdir/utf8" ] ; then
			ydb_utf8="Y"
			ydb_found_or_requested_icu_version=`icu_version`
			# Now that we determined the icu version, set ydb_icu_version env var as well.
			ydb_icu_version=$ydb_found_or_requested_icu_version
			export ydb_icu_version
		else
			ydb_utf8="N"
		fi
	fi
	# else use ydb_found_or_requested_icu_version determined from what the user had specified along with the --utf8 option
	# Check that the plugins aren't already installed or that --overwrite-existing is selected
	# Since selected --octo automatically selects --posix and --aim, we continue the install
	# without overwriting if --aim and/or --posix is already installed and --octo is selected.
	if [ "Y" != "$gtm_overwrite_existing" ] ; then
		if [ "Y" = $ydb_encplugin ] && [ -e $ydb_installdir/plugin/libgtmcrypt.so ] ; then
			echo "YDBEncrypt already installed and --overwrite-existing not specified. Exiting." ; err_exit
		fi
		if [ "Y" = $ydb_gui ] && [ -e $ydb_installdir/plugin/o/_ydbgui.so ] ; then
			echo "YDBGUI already installed and --overwrite-existing not specified. Exiting." ; err_exit
		fi
		if [ "Y" = $ydb_posix ] && [ -e $ydb_installdir/plugin/libydbposix.so ] ; then
			if [ "Y" = $ydb_octo ] ; then
				echo "YDBPosix already installed. Continuing YDBOcto install. Specify --overwrite-existing to overwrite YDBPosix."
				ydb_posix="N"
			else
				echo "YDBPosix already installed and --overwrite-existing not specified. Exiting." ; err_exit
			fi
		fi
		if [ "Y" = $ydb_aim ] && [ -e $ydb_installdir/plugin/o/_ydbaim.so ] ; then
			if [ "Y" = $ydb_octo ] ; then
				echo "YDBAIM already installed. Continuing YDBOcto install. Specify --overwrite-existing to overwrite YDBAIM."
				ydb_aim="N"
			else
				echo "YDBAIM already installed and --overwrite-existing not specified. Exiting." ; err_exit
			fi
		fi
		if [ "Y" = $ydb_zlib ] && [ -e $ydb_installdir/plugin/libgtmzlib.so ] ; then
			echo "YDBZlib already installed and --overwrite-existing not specified. Exiting." ; err_exit
		fi

		if [ "Y" = $ydb_octo ] && [ -d $ydb_installdir/plugin/octo ] ; then
			echo "YDBOcto already installed and --overwrite-existing not specified. Exiting." ; err_exit
		fi
	fi
	tmpdir=`mktmpdir`
	ydb_routines="$tmp($ydb_installdir)" ; export ydb_routines
	remove_tmpdir=1 # remove the tmpdir if the plugin installs are successful
	install_plugins
	if [ 0 = "$remove_tmpdir" ] ; then exit 1; fi	# error seen while installing one or more plugins
	rm -rf $tmpdir	# Now that we know it is safe to remove $tmpdir, do that before returning normal status
	exit 0
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
			if expr r1.36 \< "${ydb_version}" >/dev/null; then
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
		exit 1
	    fi
	fi
        tmp=`mktmpdir`
        ydb_routines="$tmp($ydb_dist)" ; export ydb_routines
        # shellcheck disable=SC2016
        if ! ydb_version=`$ydb_dist/yottadb -run %XCMD 'write $piece($zyrelease," ",2)' 2>&1`; then
            echo >&2 "$ydb_dist/yottadb -run %XCMD 'write $piece($zyrelease," ",2)' failed with output $ydb_version"
            exit 1
        fi
        rm -rf $tmp
    fi
fi
if [ "Y" = "$gtm_verbose" ] ; then
    echo Determined architecture, OS and YottaDB/GT.M version ; dump_info
    wget_flags="-P"
else wget_flags="-qP"
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
			# From r1.36 onwards, Ubuntu 22.04 is only supported (not Ubuntu 20.04).
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
				*)         continue ;;		# else move on to next tarball
			esac
			case $ydb_filename in
				*"$platform"*) ;;		# If tarball has current platform in its name, consider it
				*)             continue ;;	# else move on to next tarball
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
if [ "" != "$issystemd" ] ; then
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
    if [ -z "$icu_version" ] ; then echo n 	# Response to : "Should UTF-8 support be installed?"
    else echo y 		# Response to : "Should UTF-8 support be installed?"
        echo y 			# Response to : "Should an ICU version other than the default be used?"
        echo $ydb_found_or_requested_icu_version	# Response to : "Enter ICU version"
    fi
    if [ "Y" = $ydb_deprecated ] ; then echo y # Response to : "Should deprecated components be installed?"
    else echo n			# Response to : "Should deprecated components be installed?"
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

if [ -e configure.sh ] ; then rm -f configure.sh ; fi

tmp=`head -1 configure | cut -f 1`
if [ "#!/bin/sh" != "$tmp" ] ; then
    echo "#!/bin/sh" >configure.sh
fi
cat configure >>configure.sh
chmod +x configure.sh

if ! sh -x ./configure.sh <$gtm_configure_in 1> $gtm_tmpdir/configure_${timestamp}.out 2>$gtm_tmpdir/configure_${timestamp}.err; then
    echo "configure.sh failed. Output follows"
    cat $gtm_tmpdir/configure_${timestamp}.out $gtm_tmpdir/configure_${timestamp}.err
    exit 1
fi

rm -rf ${tmpdir:?}/*	# Now that install is successful, remove everything under temporary directory
			# We might still need this temporary directory for installing optional plugins (encplugin, posix etc.)
			# if they have been specified in the ydbinstall.sh command line. Not having a valid current directory
			# will cause YDB-E-SYSCALL errors from "getcwd()" in ydb_env_set calls made later.
if [ "Y" = "$gtm_gtm" ] ; then
	product_name="GT.M"
else
	product_name="YottaDB"
fi
echo $product_name version $ydb_version installed successfully at $ydb_installdir

# Create copies of, or links to, environment scripts and ydb & gtm executables
if [ -n "$gtm_linkenv" ] ; then
    dirensure $gtm_linkenv
    ( cd $gtm_linkenv ; rm -f ydb_env_set ydb_env_unset gtmprofile ; ln -s $ydb_installdir/ydb_env_set $ydb_installdir/ydb_env_unset $ydb_installdir/gtmprofile ./ )
    if [ "Y" = "$gtm_verbose" ] ; then echo Linked env ; ls -l $gtm_linkenv ; fi
elif [ -n "$gtm_copyenv" ] ; then
    dirensure $gtm_copyenv
    ( cd $gtm_copyenv ; rm -f ydb_env_set ydb_env_unset gtmprofile ; cp -P $ydb_installdir/ydb_env_set $ydb_installdir/ydb_env_unset $ydb_installdir/gtmprofile ./ )
    if [ "Y" = "$gtm_verbose" ] ; then echo Copied env ; ls -l $gtm_copyenv ; fi
fi
if [ -n "$gtm_linkexec" ] ; then
    dirensure $gtm_linkexec
    ( cd $gtm_linkexec ; rm -f ydb gtm ; ln -s $ydb_installdir/ydb $ydb_installdir/gtm ./ )
    if [ "Y" = "$gtm_verbose" ] ; then echo Linked exec ; ls -l $gtm_linkexec ; fi
elif [ -n "$gtm_copyexec" ] ; then
    dirensure $gtm_copyexec
    ( cd $gtm_copyexec ; rm -f ydb gtm ; cp -P $ydb_installdir/ydb $ydb_installdir/gtm ./ )
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

if [ 0 = "$remove_tmpdir" ] ; then exit 1; fi	# error seen while installing one or more plugins

rm -rf $tmpdir	# Now that we know it is safe to remove $tmpdir, do that before returning normal status
exit 0
