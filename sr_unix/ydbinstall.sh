#!/bin/sh -
#################################################################
# Copyright (c) 2014-2019 Fidelity National Information         #
# Services, Inc. and/or its subsidiaries. All rights reserved.  #
#								#
# Copyright (c) 2017-2021 YottaDB LLC and/or its subsidiaries.	#
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
#	wget https://gitlab.com/YottaDB/DB/YDB/raw/master/sr_unix/ydbinstall.sh
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
	command -v $1 >/dev/null 2>&1 || { echo >&2 "Utility [$1] is needed by ydbinstall.sh but not found. Exiting."; exit 1; }
}

# Check all utilities that ydbinstall.sh will use and ensure they are present. If not error out at beginning.
utillist="date id grep uname mktemp cut tr dirname chmod rm mkdir cat wget sed sort head basename ln gzip tar xargs sh cp"
# Check all utilities that configure.gtc (which ydbinstall.sh calls) will additionally use and ensure they are present.
# If not error out at beginning instead of erroring out midway during the install.
utillist="$utillist ps file wc touch chown chgrp groups getconf awk expr locale install ld strip"
arch=`uname -m`
if [ "armv6l" = "$arch" -o "armv7l" = "$arch" ] ; then
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

# Initialization
timestamp=`date +%Y%m%d%H%M%S`
if [ -z "$USER" ] ; then USER=`id -un` ; fi

# Functions
dump_info()
{
    set +x
    if [ -n "$gtm_arch" ] ; then echo gtm_arch " : " $gtm_arch ; fi
    if [ -n "$gtm_buildtype" ] ; then echo gtm_buildtype " : " $gtm_buildtype ; fi
    if [ -n "$gtm_configure_in" ] ; then echo gtm_configure_in " : " $gtm_configure_in ; fi
    if [ -n "$gtm_copyenv" ] ; then echo gtm_copyenv " : " $gtm_copyenv ; fi
    if [ -n "$gtm_copyexec" ] ; then echo gtm_copyexec " : " $gtm_copyexec ; fi
    if [ -n "$ydb_debug" ] ; then echo ydb_debug " : " $ydb_debug ; fi
    if [ -n "$ydb_deprecated" ] ; then echo ydb_deprecated " : " $ydb_deprecated ; fi
    if [ -n "$ydb_dist" ] ; then echo ydb_dist " : " $ydb_dist ; fi
    if [ -n "$ydb_distrib" ] ; then echo ydb_distrib " : " $ydb_distrib ; fi
    if [ -n "$gtm_dryrun" ] ; then echo gtm_dryrun " : " $gtm_dryrun ; fi
    if [ -n "$ydb_encplugin" ] ; then echo ydb_encplugin " : " $ydb_encplugin ; fi
    if [ -n "$ydb_filename" ] ; then echo ydb_filename " : " $ydb_filename ; fi
    if [ -n "$ydb_flavor" ] ; then echo ydb_flavor " : " $ydb_flavor ; fi
    if [ -n "$ydb_force_install" ] ; then echo ydb_force_install " : " $ydb_force_install ; fi
    if [ -n "$gtm_ftp_dirname" ] ; then echo gtm_ftp_dirname " : " $gtm_ftp_dirname ; fi
    if [ -n "$gtm_group" ] ; then echo gtm_group " : " $gtm_group ; fi
    if [ -n "$gtm_group_already" ] ; then echo gtm_group_already " : " $gtm_group_already ; fi
    if [ -n "$gtm_group_restriction" ] ; then echo gtm_group_restriction " : " $gtm_group_restriction ; fi
    if [ -n "$gtm_hostos" ] ; then echo gtm_hostos " : " $gtm_hostos ; fi
    if [ -n "$ydb_icu_version" ] ; then echo ydb_icu_version " : " $ydb_icu_version ; fi
    if [ -n "$gtm_install_flavor" ] ; then echo gtm_install_flavor " : " $gtm_install_flavor ; fi
    if [ -n "$ydb_installdir" ] ; then echo ydb_installdir " : " $ydb_installdir ; fi
    if [ -n "$gtm_keep_obj" ] ; then echo gtm_keep_obj " : " $gtm_keep_obj ; fi
    if [ -n "$gtm_lcase_utils" ] ; then echo gtm_lcase_utils " : " $gtm_lcase_utils ; fi
    if [ -n "$gtm_linkenv" ] ; then echo gtm_linkenv " : " $gtm_linkenv ; fi
    if [ -n "$gtm_linkexec" ] ; then echo gtm_linkexec " : " $gtm_linkexec ; fi
    if [ -n "$ydb_octo" ] ; then echo ydb_octo " : " $ydb_octo ; fi
    if [ -n "$gtm_overwrite_existing" ] ; then echo gtm_overwrite_existing " : " $gtm_overwrite_existing ; fi
    if [ -n "$ydb_posix" ] ; then echo ydb_posix " : " $ydb_posix ; fi
    if [ -n "$ydb_aim" ] ; then echo ydb_aim " : " $ydb_aim ; fi
    if [ -n "$gtm_prompt_for_group" ] ; then echo gtm_prompt_for_group " : " $gtm_prompt_for_group ; fi
    if [ -n "$gtm_sf_dirname" ] ; then echo gtm_sf_dirname " : " $gtm_sf_dirname ; fi
    if [ -n "$gtm_tmpdir" ] ; then echo gtm_tmpdir " : " $gtm_tmpdir ; fi
    if [ -n "$gtm_user" ] ; then echo gtm_user " : " $gtm_user ; fi
    if [ -n "$gtm_verbose" ] ; then echo gtm_verbose " : " $gtm_verbose ; fi
    if [ -n "$ydb_version" ] ; then echo ydb_version " : " $ydb_version ; fi
    if [ -n "$ydb_zlib" ] ; then echo ydb_zlib " : " $ydb_zlib ; fi
    if [ -n "$gtm_gtm" ] ; then echo gtm_gtm " : " $gtm_gtm ; fi
    if [ -n "$ydb_routines" ] ; then echo ydb_routines " : " $ydb_routines ; fi
    if [ -n "$timestamp" ] ; then echo timestamp " : " $timestamp ; fi
    if [ -n "$ydb_change_removeipc" ] ; then echo ydb_change_removeipc " : " $ydb_change_removeipc ; fi
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
    echo "--build-type buildtype   -> type of YottaDB build, default is pro"
    echo "--copyenv dirname        -> copy gtmprofile and gtmcshrc files to dirname; incompatible with linkenv"
    echo "--copyexec dirname       -> copy gtm script to dirname; incompatible with linkexec"
    echo "--debug                  -> turn on debugging with set -x"
    echo "--distrib dirname or URL -> source directory for YottaDB/GT.M distribution tarball, local or remote"
    echo "--dry-run                -> do everything short of installing YottaDB, including downloading the distribution"
    echo "--encplugin              -> compile and install the encryption plugin"
    echo "--filename filename      -> name of YottaDB distribution tarball"
    echo "--force-install          -> install even if the current platform is not supported"
    echo "--group group            -> group that should own the YottaDB installation"
    echo "--group-restriction      -> limit execution to a group; defaults to unlimited if not specified"
    echo "--gtm                    -> Install GT.M instead of YottaDB"
    echo "--help                   -> print this usage information"
    echo "--installdir dirname     -> directory where YottaDB is to be installed; defaults to /usr/local/lib/yottadb/version"
    echo "--keep-obj               -> keep .o files of M routines (normally deleted on platforms with YottaDB support for routines in shared libraries)"
    echo "--linkenv dirname        -> create link in dirname to gtmprofile and gtmcshrc files; incompatible with copyenv"
    echo "--nodeprecated           -> do not install deprecated components, specifically %DSEWRAP"
    echo "--linkexec dirname       -> create link in dirname to gtm script; incompatible with copyexec"
    echo "--octo parameters        -> download and install Octo; also installs required POSIX and AIM plugins. Specify optional cmake parameters for Octo as necessary"
    echo "--overwrite-existing     -> install into an existing directory, overwriting contents; defaults to requiring new directory"
    echo "--posix                  -> download and install the POSIX plugin"
    echo "--preserveRemoveIPC      -> do not allow changes to RemoveIPC in /etc/systemd/login.conf if needed; defaults to allow changes"
    echo "--prompt-for-group       -> YottaDB installation script will prompt for group; default is yes for production releases V5.4-002 or later, no for all others"
    echo "--ucaseonly-utils        -> install only upper case utility program names; defaults to both if not specified"
    echo "--user username          -> user who should own YottaDB installation; default is root"
    echo "--utf8 ICU_version       -> install UTF-8 support using specified  major.minor ICU version; specify default to use versionprovided by OS as default"
    echo "--verbose                -> output diagnostic information as the script executes; default is to run quietly"
    echo "--zlib                   -> download and install the zlib plugin"
    echo "Options that take a value (e.g, --group) can be specified as either --option=value or --option value."
    echo "Options marked with \"*\" are likely to be of interest primarily to YottaDB developers."
    echo "Version is defaulted from yottadb file if one exists in the same directory as the installer."
    echo "This version must run as root."
    echo ""
    echo "Example usages are (assumes latest YottaDB release is r1.30 and latest GT.M version is V6.3-008)"
    echo "  ydbinstall.sh                          # installs latest YottaDB release (r1.30) at /usr/local/lib/yottadb/r130"
    echo "  ydbinstall.sh --utf8 default           # installs YottaDB release r1.30 with added support for UTF-8"
    echo "  ydbinstall.sh --installdir /r130 r1.30 # installs YottaDB r1.30 at /r130"
    echo "  ydbinstall.sh --gtm                    # installs latest GT.M version (V6.3-008) at /usr/local/lib/fis-gtm/V6.3-008_x86_64"
    echo ""
    exit 1
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

# Defaults that can be over-ridden by command line options to follow
# YottaDB prefixed versions:
if [ -n "$ydb_buildtype" ] ; then gtm_buildtype="$ydb_buildtype" ; fi
if [ -n "$ydb_keep_obj" ] ; then gtm_keep_obj="$ydb_keep_obj" ; fi
if [ -n "$ydb_dryrun" ] ; then gtm_dryrun="$ydb_dryrun" ; fi
if [ -n "$ydb_group_restriction" ] ; then gtm_group_restriction="$ydb_group_restriction" ; fi
if [ -n "$ydb_gtm" ] ; then gtm_gtm="$ydb_gtm" ; fi
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
# GTM prefixed versions (for backwards compatibility)
if [ -z "$gtm_buildtype" ] ; then gtm_buildtype="pro" ; fi
if [ -z "$gtm_keep_obj" ] ; then gtm_keep_obj="N" ; fi
if [ -z "$gtm_dryrun" ] ; then gtm_dryrun="N" ; fi
if [ -z "$gtm_group_restriction" ] ; then gtm_group_restriction="N" ; fi
if [ -z "$gtm_gtm" ] ; then gtm_gtm="N" ; fi
if [ -z "$gtm_lcase_utils" ] ; then gtm_lcase_utils="Y" ; fi
if [ -z "$gtm_overwrite_existing" ] ; then gtm_overwrite_existing="N" ; fi
if [ -z "$gtm_prompt_for_group" ] ; then gtm_prompt_for_group="N" ; fi
if [ -z "$gtm_verbose" ] ; then gtm_verbose="N" ; fi


# Initializing internal flags
gtm_group_already="N"
ydb_force_install="N"

# Process command line
while [ $# -gt 0 ] ; do
    case "$1" in
        --build-type*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then gtm_buildtype=$tmp
            else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then gtm_buildtype=$2 ; shift
                else echo "--build-type needs a value" ; err_exit
                fi
            fi ;;
        --copyenv*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then gtm_copyenv=$tmp
            else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then gtm_copyenv=$2 ; shift
                else echo "--copyenv needs a value" ; err_exit
                fi
            fi
            unset gtm_linkenv
            shift ;;
        --copyexec*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then gtm_copyexec=$tmp
            else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then gtm_copyexec=$2 ; shift
                else echo "--copyexec needs a value" ; err_exit
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
	    else if [ 1 -lt "$#" ] ; then ydb_filename=$2 ; shift
		else echo "--filename needs a value" ; err_exit
		fi
	    fi
	    shift ;;
	--force-install) ydb_force_install="Y" ; shift ;;
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
                else echo "--linkenv needs a value" ; err_exit
                fi
            fi
            unset gtm_copyenv
            shift ;;
        --linkexec*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then gtm_linkexec=$tmp
            else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then gtm_linkexec=$2 ; shift
                else echo "--linkexec needs a value" ; err_exit
                fi
            fi
            unset gtm_copyexec
            shift ;;
	--nodeprecated) ydb_deprecated="N" ; shift ;;
	--octo*) tmp=`echo $1 | cut -s -d = -f 2-`
	    if [ -n "$tmp" ] ; then octo_cmake=$tmp ; fi
	    ydb_octo="Y" ;
	    ydb_posix="Y" ;
	    ydb_aim="Y";
	    shift ;;
        --overwrite-existing) gtm_overwrite_existing="Y" ; shift ;;
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
            if [ -n "$tmp" ] ; then ydb_icu_version=$tmp
            else retval=`isvaluevalid $# $2` ; if [ "$retval" -eq 0 ] ; then ydb_icu_version=`echo $2 | tr DEFAULT default`; shift
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

# Set environment variables according to machine architecture
gtm_arch=`uname -m | tr -d _`
case $gtm_arch in
    sun*) gtm_arch="sparc" ;;
esac
gtm_hostos=`uname -s | tr A-Z a-z`
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

osfile="/etc/os-release"
osid=`grep -w ID $osfile | cut -d= -f2 | cut -d'"' -f2`
if [ "N" = "$ydb_force_install" ]; then
	# At this point, we know the current machine architecture is supported by YottaDB
	# but not yet sure if the OS and/or version is supported. Since
	# --force-install is not specified, it is okay to do the os-version check now.
	osver_supported=0 # Consider platform unsupported by default
	isdebianbusteronx8664=0	# Set current platform to not be Debian 10 buster on x86_64 by default
	if [ -f "$osfile" ] ; then
		osver=`grep -w VERSION_ID $osfile | cut -d= -f2 | cut -d'"' -f2`
		# Set an impossible major/minor version by default in case we do not descend down known platforms in if/else below.
		osallowmajorver="999"
		osallowminorver="999"
		if [ "x8664" = "${ydb_flavor}" ] ; then
			if [ "ubuntu" = "${osid}" ] ; then
				# Ubuntu 18.04 and onwards is considered supported on 64-bit x86 architecture
				osallowmajorver="18"
				osallowminorver="04"
			elif [ "rhel" = "${osid}" ] ; then
				# RHEL 7.x is considered supported on 64-bit x86 architecture
				osallowmajorver="7"
				osallowminorver="0"
			elif [ "centos" = "${osid}" ] ; then
				# CentOS 8.x is considered supported on 64-bit x86 architecture
				osallowmajorver="8"
				osallowminorver="0"
			elif [ "debian" = "${osid}" ] ; then
				# Only Debian 10 (buster) is considered supported on 64-bit x86 architecture for now.
				# Note that the stable distribution is "buster" and its unstable variety is "buster/sid".
				# We support either at this point hence the -o check below.
				prettyname=`grep -w PRETTY_NAME $osfile | cut -d= -f2 | cut -d'"' -f2`
				if [ "Debian GNU/Linux buster/sid" = "${prettyname}" -o "Debian GNU/Linux 10 (buster)" = "${prettyname}" ] ; then
					isdebianbusteronx8664=1
					osallowmajorver="10"
					osallowminorver="0"
				fi
			fi
		elif [ "aarch64" = "${ydb_flavor}" ] ; then
			if [ "ubuntu" = "${osid}" ] ; then
				# Ubuntu 18.04 and onwards is considered supported on 64-bit ARM architecture
				osallowmajorver="18"
				osallowminorver="04"
			fi
		else
			if [ "armv6l" = "${ydb_flavor}" -o "armv7l" = "${ydb_flavor}" ] ; then
				if [ "raspbian" = "${osid}" -o "debian" = ${osid} ] ; then
					# Raspbian or Debian 9 or 9.x is considered supported on 32-bit ARM architecture
					osallowmajorver="9"
					osallowminorver="0"
				fi
			fi
		fi
		osmajorver=`echo $osver | cut -d. -f1`
		# It is possible there is no major version (e.g. Debian 10 buster/sid). In that case, set major version as 10.
		if [ "" = "$osmajorver" -a 1 = $isdebianbusteronx8664 ] ; then
			osmajorver="10"
		fi
		# It is possible there is no minor version (e.g. Raspbian 9) in which case "cut" will not work
		# as -f2 will give us 9 again. So use awk in that case which will give us "" as $2.
		osminorver=`echo $osver | awk -F. '{print $2}'`
		if [ "" = "$osminorver" ] ; then
			# Needed by "expr" (since it does not compare "" vs numbers correctly)
			# in case there is no minor version field (e.g. Raspbian 9 or even Debian 10 buster/sid).
			osminorver="0"
		fi
		if [ 1 = `expr "$osmajorver" ">" "$osallowmajorver"` ] ; then
			osver_supported=1
		elif [ 1 = `expr "$osmajorver" "=" "$osallowmajorver"` -a 1 = `expr "$osminorver" ">=" "$osallowminorver"` ] ; then
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
	else
		echo "/etc/os-release does not exist on host; Not installing YottaDB."
	fi
	if [ 0 = "$osver_supported" ] ; then
		echo "Specify ydbinstall.sh --force-install to force install"
		err_exit
	fi

	# Use the OS variables just defined to determine if we are running on Ubuntu 18.10 or greater and make sure libtinfo5 is installed
	if [ "ubuntu" = "${osid}" -a 1 = `expr "$osmajorver" ">" "18"` ] || [ 1 = `expr "$osmajorver" "=" "18"` -a 1 = `expr "$osminorver" ">=" "10"` ] ; then
		if [ "x8664" = "${ydb_flavor}" -a ! -f /lib/x86_64-linux-gnu/libtinfo.so.5 ] || [ "aarch64" = "${ydb_flavor}" -a ! -f /lib/aarch64-linux-gnu/libtinfo.so.5 ] ; then
			echo "libtinfo5 package is required to be installed on Ubuntu 18.10 or greater using 'sudo apt-get install --no-install-recommends libtinfo5'"
			err_exit
		fi
	fi
fi

# YottaDB version is required - first see if ydbinstall and yottadb/mumps are bundled
if [ -z "$ydb_version" ] ; then
    tmp=`dirname $0`
    if [ \( -e "$tmp/yottadb" -o -e "$tmp/mumps" \) -a -e "$tmp/_XCMD.m" ] ; then
        ydb_distrib=$tmp
        ydb_dist=$tmp ; export ydb_dist
        chmod +x $ydb_dist/yottadb
        tmp=`mktmpdir`
        ydb_routines="$tmp($ydb_dist)" ; export ydb_routines
        ydb_version=`$ydb_dist/yottadb -run %XCMD 'write $piece($zyrelease," ",2)' 2>&1`
	if [ $? -gt 0 ] ; then echo >&2 "$ydb_dist/yottadb -run %XCMD 'write $piece($zyrelease," ",2)' failed with output $ydb_version"; exit 1; fi
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
if [ -z "$ydb_version" -o "latest" = "$latest" ] ; then
    case $ydb_distrib in
        http://sourceforge.net/projects/fis-gtm | https://sourceforge.net/projects/fis-gtm)
            gtm_gtm="Y"
            if [ "Y" = "$gtm_verbose" ] ; then
               echo wget ${ydb_distrib}/files/${gtm_sf_dirname}/latest to determine latest version
               echo Check proxy settings if wget hangs
            fi
            if { wget $wget_flags $gtm_tmpdir ${ydb_distrib}/files/${gtm_sf_dirname}/latest 2>&1 1>${gtm_tmpdir}/wget_latest.log ; } ; then
                ydb_version=`cat ${gtm_tmpdir}/latest`
            else echo Unable to determine YottaDB/GT.M version ; err_exit
            fi ;;
        ftp://*)
            if [ "Y" = "$gtm_verbose" ] ; then
               echo wget $gtm_tmpdir ${ydb_distrib}/${gtm_ftp_dirname}/latest to determine latest version
               echo Check proxy settings if wget hangs
            fi
            if { wget $wget_flags $gtm_tmpdir ${ydb_distrib}/${gtm_ftp_dirname}/latest 2>&1 1>${gtm_tmpdir}/wget_latest.log ; } ; then
                ydb_version=`cat ${gtm_tmpdir}/latest`
            else echo Unable to determine YottaDB/GT.M version ; err_exit
            fi ;;
        https://gitlab.com/api/*)
            if [ "Y" = "$gtm_verbose" ] ; then
                echo wget ${ydb_distrib} to determine latest version
                echo Check proxy settings if wget hangs
            fi
            if { wget $wget_flags $gtm_tmpdir ${ydb_distrib} 2>&1 1>${gtm_tmpdir}/wget_latest.log ; } ; then
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
                	2>&1 1>${gtm_tmpdir}/wget_dist.log ; } ; then
                echo Unable to download GT.M distribution $ydb_filename ; err_exit
            fi ;;
        https://gitlab.com/api/*)
            if [ "Y" = "$gtm_verbose" ] ; then
                echo wget ${ydb_distrib}/${ydb_version} and parse to download tarball
                echo Check proxy settings if wget hangs
            fi
            if { wget $wget_flags $gtm_tmpdir ${ydb_distrib}/${ydb_version} 2>&1 1>${gtm_tmpdir}/wget_dist.log ;} ; then
		# There might be multiple binary tarballs of YottaDB (for various architectures & platforms).
		# If so, choose the one that corresponds to the current host.
		yottadb_download_urls=`sed 's,/uploads/,\n&,g' ${gtm_tmpdir}/${ydb_version} | grep "^/uploads/" | cut -d')' -f1`
		# Determine current host's architecture
		arch=`uname -m | tr -d '_'`
		# Determine current host's OS. We expect the OS name in the tarball.
		platform=`uname -s | tr '[A-Z]' '[a-z]'`
		if [ $arch = "x8664" ] ; then
			# If the current architecture is x86_64 and the distribution is RHEL (including CentOS and SLES)
			# or Debian then set the platform to rhel or debian (not linux) as there are specific tarballs
			# for these distributions. If the distribution is Ubuntu, then set the platform to ubuntu (not linux)
			# if the version is 20.04 or later as there is a specific tarball for newer versions of Ubuntu.
			#
			# To get the correct binary for CentOS, RHEL and SLES, we treat OS major version 7 as rhel and later versions as centos
			if [ "rhel" = "${osid}" -o "centos" = "${osid}" -o "sles" = "${osid}" ] ; then
				# CentOS-specific releases of YottaDB for x86_64 happened only after r1.26
				if [ "r1.26" \< "${ydb_version}" ] ; then
					# If the OS major version is later than 7, treat it as centos. Otherwise, treat it as rhel.
					osmajorver=`echo $osver | cut -d. -f1`
					if [ 1 = `expr "$osmajorver" ">" "7"` ] ; then
						platform="centos"
					else
						platform="rhel"
					fi
				# RHEL-specific releases of YottaDB for x86_64 happened only starting r1.10 so do this
				# only if the requested version is not r1.00 (the only YottaDB release prior to r1.10)
				elif [ "r1.00" != ${ydb_version} ] ; then
					platform="rhel"
				fi
			elif [ "debian" = "${osid}" ] ; then
				# Debian-specific releases of YottaDB for x86_64 happened only after r1.24
				if [ "r1.24" \< "${ydb_version}" ]; then
					platform="debian"
				fi
			elif [ "ubuntu" = "${osid}" ] ; then
				# Starting with r1.30, there is an Ubuntu 20.04 build where the platform is ubuntu (not linux)
				# so set the platform to ubuntu only if the requested version is r1.30 or later and the
				# Ubuntu version is 20.04 or later.
				if [ "r1.28" \< "${ydb_version}" ]; then
					# If the OS major version is 20 or later, treat it as ubuntu. Otherwise, treat it as linux.
					osmajorver=`echo $osver | cut -d. -f1`
					if [ 1 = `expr "$osmajorver" ">" "19"` ] ; then
						platform="ubuntu"
					fi
				fi
			fi
		fi
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
				*"$platform"*) ;;		# If tarball has current architecture in its name, consider it
				*)             continue ;;	# else move on to next tarball
			esac
			yottadb_download_url="https://gitlab.com/YottaDB/DB/YDB${fullfilename}"
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
                	2>&1 1>${gtm_tmpdir}/wget_dist.log ; } ; then
                echo Unable to download GT.M distribution $ydb_filename ; err_exit
            fi ;;
        *)
            if [ -f ${ydb_distrib}/${ydb_filename} ] ; then
                if [ "Y" = "$gtm_verbose" ] ; then echo tarball is ${ydb_distrib}/${ydb_filename} ; fi
                ln -s ${ydb_distrib}/${ydb_filename} $gtm_tmpdir
            else echo Unable to locate YottaDB/GT.M distribution file ${ydb_distrib}/${ydb_filename} ; err_exit
            fi ;;
    esac
    ( cd $gtm_tmpdir/tmp ; gzip -d < ${gtm_tmpdir}/${ydb_filename} | tar xf - 2>&1 1>${gtm_tmpdir}/tar.log )
fi
if [ "Y" = "$gtm_verbose" ] ; then echo Downloaded and unpacked YottaDB/GT.M distribution ; dump_info ; fi

# Check installation settings & provide defaults as needed
tmp=`id -un`
if [ -z "$gtm_user" ] ; then gtm_user=$tmp
else if [ "$gtm_user" != "`id -un $gtm_user`" ] ; then
    echo $gtm_user is a non-existent user ; err_exit
    fi
fi
if [ "root" = $tmp ] ; then
    if [ -z "$gtm_group" ] ; then gtm_group=`id -gn`
    else if [ "root" != "$gtm_user" -a "$gtm_group" != "`id -Gn $gtm_user | xargs -n 1 | grep $gtm_group`" ] ; then
        echo $gtm_user is not a member of $gtm_group ; err_exit
        fi
    fi
 else
    echo Non-root installations not currently supported
    if [ "N" = "$gtm_dryrun" ] ; then err_exit
    else echo "Continuing because --dry-run selected"
    fi
fi
if [ -z "$ydb_installdir" ] ; then
    if [ "N" = "$gtm_gtm" ] ; then
         ydbver=`echo $ydb_version | tr '[A-Z]' '[a-z]' | tr -d '.-'`
         ydb_installdir=/usr/local/lib/yottadb/${ydbver}
    else ydb_installdir=/usr/local/lib/fis-gtm/${ydb_version}_${gtm_install_flavor}
    fi
fi
# if install directory is relative then need to make it absolute before passing it to configure
# (or else configure will create a subdirectory under $tmpdir (/tmp/.*) and install YottaDB there which is not what we want)
if [ `echo $ydb_installdir | grep -c '^/'` -eq 0 ] ; then
    ydb_installdir=`pwd`/$ydb_installdir
fi
if [ -d "$ydb_installdir" -a "Y" != "$gtm_overwrite_existing" ] ; then
    echo $ydb_installdir exists and --overwrite-existing not specified ; err_exit
fi

if [ "Y" = "$gtm_verbose" ] ; then echo Finished checking options and assigning defaults ; dump_info ; fi

# Prepare input to YottaDB configure script. The corresponding questions in configure.gtc are listed below in comments
gtm_configure_in=${gtm_tmpdir}/configure_${timestamp}.in
export ydb_change_removeipc			# Signal configure.gtc to set RemoveIPC=no or not, if needed
echo $gtm_user >>$gtm_configure_in		# Response to : "What user account should own the files?"
echo $gtm_group >>$gtm_configure_in		# Response to : "What group should own the files?"
echo $gtm_group_restriction >>$gtm_configure_in	# Response to : "Should execution of YottaDB be restricted to this group?"
echo $ydb_installdir >>$gtm_configure_in	# Response to : "In what directory should YottaDB be installed?"
echo y >>$gtm_configure_in			# Response to one of two possible questions
						#	"Directory $ydb_dist exists. If you proceed with this installation then some files will be over-written. Is it ok to proceed?"
						#	"Directory $ydb_dist does not exist. Do you wish to create it as part of this installation? (y or n)"
if [ -z "$ydb_icu_version" ] ; then echo n  >>$gtm_configure_in	# Response to : "Should UTF-8 support be installed?"
else echo y  >>$gtm_configure_in		# Response to : "Should UTF-8 support be installed?"
    if [ "default" = $ydb_icu_version ] ; then echo n  >>$gtm_configure_in	# Response to : "Should an ICU version other than the default be used?"
    else echo y >>$gtm_configure_in		# Response to : "Should an ICU version other than the default be used?"
        echo $ydb_icu_version >>$gtm_configure_in	# Response to : "Enter ICU version"
    fi
fi
if [ "Y" = $ydb_deprecated ] ; then echo y >>$gtm_configure_in # Response to : "Should deprecated components be installed?"
else echo n >>$gtm_configure_in			# Response to : "Should deprecated components be installed?"
fi
echo $gtm_lcase_utils >>$gtm_configure_in	# Response to : "Do you want uppercase and lowercase versions of the MUMPS routines?"
if [ "Y" = $gtm_shlib_support ] ; then echo $gtm_keep_obj >>$gtm_configure_in ; fi	# Response to : "Object files of M routines placed in shared library $ydb_dist/libyottadbutil$ext. Keep original .o object files (y or n)?"
echo n >>$gtm_configure_in			# Response to : "Installation completed. Would you like all the temporary files removed from this directory?"
if [ "Y" = "$gtm_verbose" ] ; then echo Prepared configuration file ; cat $gtm_configure_in ; dump_info ; fi


# Run the YottaDB configure script
if [ "$ydb_distrib" != "$gtm_tmpdir" ] ; then
    chmod +w $gtm_tmpdir/tmp
    cd $gtm_tmpdir/tmp
    # Starting YottaDB r1.10, unpacking the binary tarball creates an additional directory (e.g. yottadb_r122)
    # before the untar so cd into that subdirectory to get at the "configure" script from the distribution.
    if [ "N" = "$gtm_gtm" -a "r1.00" != ${ydb_version} ] ; then
        cd yottadb_r*
    fi
fi

if [ -e configure.sh ] ; then rm -f configure.sh ; fi

tmp=`head -1 configure | cut -f 1`
if [ "#!/bin/sh" != "$tmp" ] ; then
    echo "#!/bin/sh" >configure.sh
fi
cat configure >>configure.sh
chmod +x configure.sh

# Stop here if this is a dry run
if [ "Y" = "$gtm_dryrun" ] ; then echo Installation prepared in $gtm_tmpdir ; exit ; fi

sh -x ./configure.sh <$gtm_configure_in 1> $gtm_tmpdir/configure_${timestamp}.out 2>$gtm_tmpdir/configure_${timestamp}.err
if [ $? -gt 0 ] ; then echo "configure.sh failed. Output follows"; cat $gtm_tmpdir/configure_${timestamp}.out $gtm_tmpdir/configure_${timestamp}.err ; exit 1; fi

rm -rf $tmpdir/*	# Now that install is successful, remove everything under temporary directory
			# We might still need this temporary directory for installing optional plugins (encplugin, posix etc.)
			# if they have been specified in the ydbinstall.sh command line. Not having a valid current directory
			# will cause YDB-E-SYSCALL errors from "getcwd()" in ydb_env_set calls made later.
if [ "Y" = "$gtm_gtm" ] ; then
	product_name="GT.M"
else
	product_name="YottaDB"
fi
echo $product_name version $ydb_version installed successfully at $ydb_installdir

# Create copies of environment scripts and gtm executable
if [ -d "$gtm_linkenv" ] ; then
    ( cd $gtm_linkenv ; ln -s $ydb_installdir/ydb_env_set $ydb_installdir/ydb_env_unset $ydb_installdir/gtmprofile ./ )
    if [ "Y" = "$gtm_verbose" ] ; then echo Linked env ; ls -l $gtm_linkenv ; fi
else if [ -d "$gtm_copyenv" ] ; then
        ( cd $gtm_copyenv ; cp -P $ydb_installdir/ydb_env_set $ydb_installdir/ydb_env_unset $ydb_installdir/gtmprofile ./ )
        if [ "Y" = "$gtm_verbose" ] ; then echo Copied env ; ls -l $gtm_copyenv ; fi
     fi
fi
if [ -d "$gtm_linkexec" ] ; then
    ( cd $gtm_linkexec ; ln -s $ydb_installdir/ydb $ydb_installdir/gtm ./ )
    if [ "Y" = "$gtm_verbose" ] ; then echo Linked exec ; ls -l $gtm_linkexec ; fi
else if [ -d "$gtm_copyexec" ] ; then
        ( cd $gtm_copyexec ; cp -P $ydb_installdir/ydb $ydb_installdir/gtm ./ )
        if [ "Y" = "$gtm_verbose" ] ; then echo Copied exec ; ls -l $gtm_copyexec ; fi
     fi
fi

# Create the pkg-config file
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
	! [ $existing_version \> $(echo $ydb_version) ];
}; then
    cp ${ydb_installdir}/yottadb.pc ${pcfilepath}/yottadb.pc
    echo $product_name pkg-config file installed successfully at ${pcfilepath}/yottadb.pc
else
    echo Skipping $product_name pkg-config file install for ${ydb_version} as newer version $existing_version exists at ${pcfilepath}/yottadb.pc
fi

# install optional components if they were selected

remove_tmpdir=1	# It is okay to remove $tmpdir at the end assuming ydbinstall is successful at installing plugins.
		# Any errors while installing any plugin will set this variable to 0.

if [ "Y" = $ydb_posix ] ; then
	cd $tmpdir	# Get back to top level temporary directory as the current directory for ydb_env_set
	. ${ydb_installdir}/ydb_env_set
	mkdir posix_tmp
	cd posix_tmp
	if curl -fSsLO https://gitlab.com/YottaDB/Util/YDBPosix/-/archive/master/YDBPosix-master.tar.gz; then
		tar xzf YDBPosix-master.tar.gz
		cd YDBPosix-master
		mkdir build && cd build
		cmake ..
		if make -j `grep -c ^processor /proc/cpuinfo` && sudo make install; then
			# Save the build directory if either of the make commands return a non-zero exit code. Otherwise, remove it.
			cd ../../..
			. ${ydb_installdir}/ydb_env_unset
			sudo rm -R posix_tmp
		else
			echo "POSIX plugin build failed. The build directory ($PWD/posix_tmp) has been saved."
			remove_tmpdir=0
		fi
	else
		echo "POSIX plugin build failed. Unable to download the POSIX plugin. Your internet connection and/or the gitlab servers may be down. Please try again later."
	fi
fi

if [ "Y" = $ydb_aim ] ; then
	cd $tmpdir
	mkdir aim_tmp && cd aim_tmp
	url="https://gitlab.com/YottaDB/Util/YDBAIM.git"
	if git clone ${url} .; then
		if ! ./install.sh; then
			echo "YDBAIM Installation failed."
			remove_tmpdir=0
		fi
	else
		echo "Failed to download YDBAIM. Check your internet connection. URL is ${url}"
	fi
fi

if [ "Y" = $ydb_encplugin ] ; then
	cd $tmpdir	# Get back to top level temporary directory as the current directory for ydb_env_set
	export ydb_icu_version=$ydb_icu_version
	mkdir enc_tmp
	cd enc_tmp
	sudo tar -xf ${ydb_installdir}/plugin/gtmcrypt/source.tar
	sudo ydb_dist=${ydb_installdir} make -j `grep -c ^processor /proc/cpuinfo`
	if sudo ydb_dist=${ydb_installdir} make install; then
		# Save the build directory if the make install command returns a non-zero exit code. Otherwise, remove it.
		cd ..
		sudo rm -R enc_tmp
	else
		echo "Encryption plugin build failed. The build directory ($PWD/enc_tmp) has been saved."
		remove_tmpdir=0
	fi
	# rename gtmcrypt to ydbcrypt and create a symbolic link for backward compatibility
	mv ${ydb_installdir}/plugin/gtmcrypt ${ydb_installdir}/plugin/ydbcrypt
	ln -s ${ydb_installdir}/plugin/ydbcrypt ${ydb_installdir}/plugin/gtmcrypt
fi

if [ "Y" = $ydb_zlib ] ; then
	cd $tmpdir	# Get back to top level temporary directory as the current directory for ydb_env_set
	. ${ydb_installdir}/ydb_env_set
	mkdir zlib_tmp
	cd zlib_tmp
	if curl -fSsLO https://gitlab.com/YottaDB/Util/YDBZlib/-/archive/master/YDBZlib-master.tar.gz; then
		tar xzf YDBZlib-master.tar.gz
		cd YDBZlib-master
		if gcc -c -fPIC -I${ydb_installdir} gtmzlib.c && gcc -o libgtmzlib.so -shared gtmzlib.o; then
			# Save the build directory if either of the gcc commands return a non-zero exit code. Otherwise, remove it.
			sudo cp gtmzlib.xc libgtmzlib.so ${ydb_installdir}/plugin
			sudo cp _ZLIB.m ${ydb_installdir}/plugin/r
			${ydb_installdir}/mumps ${ydb_installdir}/plugin/r/_ZLIB
			if [ "Y" = $ydb_utf8 ] ; then
				if [ "default" = "$ydb_icu_version" ] ; then
					ydb_icu_version=`pkg-config --modversion icu-io`
				fi
				mkdir utf8
				cd utf8
				export ydb_chset="UTF-8"
				. ${ydb_installdir}/utf8/ydb_env_set
				${ydb_installdir}/mumps ${ydb_installdir}/plugin/r/_ZLIB
				sudo cp _ZLIB.o ${ydb_installdir}/plugin/o/utf8
				. ${ydb_installdir}/ydb_env_unset
				cd ..
			fi
			sudo cp _ZLIB.o ${ydb_installdir}/plugin/o
			. ${ydb_installdir}/ydb_env_unset
			cd ../..
			sudo rm -R zlib_tmp
		else
			echo "zlib plugin build failed. The build directory ($PWD/zlib_tmp) has been saved."
			remove_tmpdir=0
		fi
	else
		echo "zlib plugin build failed. Unable to download the zlib plugin. Your internet connection and/or the gitlab servers may be down. Please try again later."
	fi
fi

if [ "Y" = $ydb_octo ] ; then
	cd $tmpdir	# Get back to top level temporary directory as the current directory for ydb_env_set
	export ydb_dist=${ydb_installdir}
	if git clone https://gitlab.com/YottaDB/DBMS/YDBOcto YDBOcto-master; then
		cd YDBOcto-master
		mkdir build
		cd build
		if [ "rhel" = "${osid}" -o "centos" = "${osid}" -o "sles" = "${osid}" ] ; then
			cmake3 ${octo_cmake} ..
		else
			cmake ${octo_cmake} ..
		fi
		if make -j `grep -c ^processor /proc/cpuinfo` && sudo -E make install; then
			# Save the build directory if either of the make commands return a non-zero exit code. Otherwise, remove it.
			cd ../..
			sudo rm -R YDBOcto-master
		else
			echo "Octo build failed. The build directory ($PWD/YDBOcto-master) and the tarball ($PWD/YDBOcto-master.tar.gz) have been saved."
			remove_tmpdir=0
		fi
	else
		echo "Octo build failed. Unable to download Octo. Your internet connection and/or the gitlab servers may be down. Please try again later."
	fi
fi

if [ 0 = "$remove_tmpdir" ] ; then exit 1; fi	# error seen while installing one or more plugins

rm -rf $tmpdir	# Now that we know it is safe to remove $tmpdir, do that before returning normal status
exit 0
