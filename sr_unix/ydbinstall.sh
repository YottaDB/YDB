#!/bin/sh -
#################################################################
# Copyright (c) 2014-2017 Fidelity National Information         #
# Services, Inc. and/or its subsidiaries. All rights reserved.  #
#								#
# Copyright (c) 2017,2018 YottaDB LLC. and/or its subsidiaries.	#
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

# This script automates the installation of YottaDB as much as possible,
# to the extent of attempting to download the distribution file.
# Current limitation is GNU/Linux on x86 (32- & 64-bit) architectures
# and root installation, but it is intended to relax this in the future.

# NOTE: This script requires the GNU wget program to download
# distribution files that are not on the local file system.

# Revision history
#
# 2011-02-15  0.01 K.S. Bhaskar - Initial version for internal use
# 2011-02-20  0.02 K.S. Bhaskar - Mostly usable with enough features for first Beta test
# 2011-02-21  0.03 K.S. Bhaskar - Deal with case of no group bin, bug fixes, download from FTP site, other platforms
# 2011-02-28  0.04 K.S. Bhaskar - Use which to get locations of id and grep, more bug fixes
# 2011-03-05  0.05 K.S. Bhaskar - Through V5.4-001 group only needed if execution restricted to a group
# 2011-03-08  0.06 K.S. Bhaskar - Make it work when bundled with GT.M V5.4-002
# 2011-03-10  0.10 K.S. Bhaskar - Incorporate review comments to bundle with V5.4-002 distribution
# 2011-05-03  0.11 K.S. Bhaskar - Allow for letter suffix releases
# 2011-10-25  0.12 K.S. Bhaskar - Support option to delete .o files on shared library platforms
# 2014-08-13  0.13 K.S. Bhaskar - Add verbosity around getting latest version and tarball, if requested
# 2015-10-13  0.14 GT.M Staff   - Fix a few minor bugs
# 2017-07-16  0.15 Sam Habiel   - --yottadb or --distrib https://github.com/YottaDB/YottaDB to install YottaDB
# 2017-08-12  0.16 Christopher Edwards - Default to YottaDB
# 2017-10-xx  0.17 Narayanan Iyer - See git commit message for description of changes.
#	Going forward, this script is maintained at https://github.com/YottaDB/YottaDB/blob/master/sr_unix/ydbinstall.sh
#	and there is no revision history in this file.

# Turn on debugging if set
if [ "Y" = "$ydb_debug" ] ; then set -x ; fi

# Initialization
timestamp=`date +%Y%m%d%H%M%S`
ydb_id=`which id`
ydb_grep=`which grep`
if [ -z "$USER" ] ; then USER=`$ydb_id -un` ; fi

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
    if [ -n "$ydb_dist" ] ; then echo ydb_dist " : " $ydb_dist ; fi
    if [ -n "$ydb_distrib" ] ; then echo ydb_distrib " : " $ydb_distrib ; fi
    if [ -n "$gtm_dryrun" ] ; then echo gtm_dryrun " : " $gtm_dryrun ; fi
    if [ -n "$ydb_filename" ] ; then echo ydb_filename " : " $ydb_filename ; fi
    if [ -n "$ydb_flavor" ] ; then echo ydb_flavor " : " $ydb_flavor ; fi
    if [ -n "$gtm_ftp_dirname" ] ; then echo gtm_ftp_dirname " : " $gtm_ftp_dirname ; fi
    if [ -n "$gtm_group" ] ; then echo gtm_group " : " $gtm_group ; fi
    if [ -n "$gtm_group_already" ] ; then echo gtm_group_already " : " $gtm_group_already ; fi
    if [ -n "$gtm_group_restriction" ] ; then echo gtm_group_restriction " : " $gtm_group_restriction ; fi
    if [ -n "$gtm_hostos" ] ; then echo gtm_hostos " : " $gtm_hostos ; fi
    if [ -n "$gtm_icu_version" ] ; then echo gtm_icu_version " : " $gtm_icu_version ; fi
    if [ -n "$gtm_install_flavor" ] ; then echo gtm_install_flavor " : " $gtm_install_flavor ; fi
    if [ -n "$ydb_installdir" ] ; then echo ydb_installdir " : " $ydb_installdir ; fi
    if [ -n "$gtm_keep_obj" ] ; then echo gtm_keep_obj " : " $gtm_keep_obj ; fi
    if [ -n "$gtm_lcase_utils" ] ; then echo gtm_lcase_utils " : " $gtm_lcase_utils ; fi
    if [ -n "$gtm_linkenv" ] ; then echo gtm_linkenv " : " $gtm_linkenv ; fi
    if [ -n "$gtm_linkexec" ] ; then echo gtm_linkexec " : " $gtm_linkexec ; fi
    if [ -n "$gtm_overwrite_existing" ] ; then echo gtm_overwrite_existing " : " $gtm_overwrite_existing ; fi
    if [ -n "$gtm_prompt_for_group" ] ; then echo gtm_prompt_for_group " : " $gtm_prompt_for_group ; fi
    if [ -n "$gtm_sf_dirname" ] ; then echo gtm_sf_dirname " : " $gtm_sf_dirname ; fi
    if [ -n "$gtm_tmp" ] ; then echo gtm_tmp " : " $gtm_tmp ; fi
    if [ -n "$gtm_user" ] ; then echo gtm_user " : " $gtm_user ; fi
    if [ -n "$gtm_verbose" ] ; then echo gtm_verbose " : " $gtm_verbose ; fi
    if [ -n "$ydb_version" ] ; then echo ydb_version " : " $ydb_version ; fi
    if [ -n "$gtm_gtm" ] ; then echo gtm_gtm " : " $gtm_gtm ; fi
    if [ -n "$gtmroutines" ] ; then echo gtmroutines " : " $gtmroutines ; fi
    if [ -n "$timestamp" ] ; then echo timestamp " : " $timestamp ; fi
    if [ -n "$ydb_change_removeipc" ] ; then echo ydb_change_removeipc " : " $ydb_change_removeipc ; fi
    if [ "Y" = "$ydb_debug" ] ; then set -x ; fi
}

err_exit()
{
	echo "YottaDB installation aborted due to above error. Run ydbinstall --help for detailed option list"
	exit 1
}

help_exit()
{
    set +x
    echo "ydbinstall [option] ... [version]"
    echo "Options are:"
    echo "--build-type buildtype - * type of YottaDB build, default is pro"
    echo "--copyenv dirname - copy gtmprofile and gtmcshrc files to dirname; incompatible with linkenv"
    echo "--copyexec dirname - copy gtm script to dirname; incompatible with linkexec"
    echo "--debug - * turn on debugging with set -x"
    echo "--distrib dirname or URL - source directory for YottaDB/GT.M distribution tarball, local or remote"
    echo "--preserveRemoveIPC - do not allow changes to RemoveIPC in /etc/systemd/login.conf if needed; defaults to allow changes"
    echo "--dry-run - do everything short of installing YottaDB, including downloading the distribution"
    echo "--group group - group that should own the YottaDB installation"
    echo "--group-restriction - limit execution to a group; defaults to unlimited if not specified"
    echo "--gtm - Install GT.M instead of YottaDB"
    echo "--help - print this usage information"
    echo "--installdir dirname - directory where YottaDB is to be installed; defaults to /usr/local/lib/yottadb/version"
    m1="--keep-obj - keep .o files"
    m1="$m1"" of M routines (normally deleted on platforms with YottaDB support for routines in shared libraries)"
    echo "$m1"
    echo "--linkenv dirname - create link in dirname to gtmprofile and gtmcshrc files; incompatible with copyenv"
    echo "--linkexec dirname - create link in dirname to gtm script; incompatible with copyexec"
    echo "--overwrite-existing - install into an existing directory, overwriting contents; defaults to requiring new directory"
    m1="--prompt-for-group - * YottaDB installation "
    m1="$m1""script will prompt for group; default is yes for production releases V5.4-002 or later, no for all others"
    echo "$m1"
    echo "--ucaseonly-utils -- install only upper case utility program names; defaults to both if not specified"
    echo "--user username - user who should own YottaDB installation; default is root"
    m1="--utf8 ICU_version - install "
    m1="$m1""UTF-8 support using specified  major.minor ICU version; specify default to use default version"
    echo "$m1"
    echo "--verbose - * output diagnostic information as the script executes; default is to run quietly"
    echo "options that take a value (e.g, --group) can be specified as either --option=value or --option value"
    echo "options marked with * are likely to be of interest primarily to YottaDB developers"
    echo "version is defaulted from mumps file if one exists in the same directory as the installer"
    echo "This version must run as root."
    echo ""
    echo "Example usages are (assumes latest YottaDB release is r1.10 and latest GT.M version is V6.3-002)"
    echo "  ydbinstall.sh                          # installs latest YottaDB release (r1.10) at /usr/local/lib/yottadb/r110"
    echo "  ydbinstall.sh --utf8 default           # installs YottaDB release r1.10 with added support for UTF-8"
    echo "  ydbinstall.sh --installdir /r110 r1.10 # installs YottaDB r1.10 at /r110"
    echo "  ydbinstall.sh --gtm                    # installs latest GT.M version (V6.3-002) at /usr/local/lib/fis-gtm/V6.3-002_x86_64"
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

# Defaults that can be over-ridden by command line options to follow
if [ -z "$gtm_buildtype" ] ; then gtm_buildtype="pro" ; fi
if [ -z "$gtm_keep_obj" ] ; then gtm_keep_obj="N" ; fi
if [ -z "$gtm_dryrun" ] ; then gtm_dryrun="N" ; fi
if [ -z "$gtm_group_restriction" ] ; then gtm_group_restriction="N" ; fi
if [ -z "$gtm_gtm" ] ; then gtm_gtm="N" ; fi
if [ -z "$gtm_lcase_utils" ] ; then gtm_lcase_utils="Y" ; fi
if [ -z "$gtm_overwrite_existing" ] ; then gtm_overwrite_existing="N" ; fi
if [ -z "$gtm_prompt_for_group" ] ; then gtm_prompt_for_group="N" ; fi
if [ -z "$gtm_verbose" ] ; then gtm_verbose="N" ; fi
if [ -z "$ydb_change_removeipc" ] ; then ydb_change_removeipc="yes" ; fi

# Initializing internal flags
gtm_group_already="N"

# Process command line
while [ $# -gt 0 ] ; do
    case "$1" in
        --build-type*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then gtm_buildtype=$tmp
            else if [ 1 -lt "$#" ] ; then gtm_buildtype=$2 ; shift
                else echo "--buildtype needs a value" ; err_exit
                fi
            fi ;;
        --copyenv*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then gtm_copyenv=$tmp
            else if [ 1 -lt "$#" ] ; then gtm_copyenv=$2 ; shift
                else echo "--copyenv needs a value" ; err_exit
                fi
            fi
            unset gtm_linkenv
            shift ;;
        --copyexec*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then gtm_copyexec=$tmp
            else if [ 1 -lt "$#" ] ; then gtm_copyexec=$2 ; shift
                else echo "--copyexec needs a value" ; err_exit
                fi
            fi
            unset gtm_linkexec
            shift ;;
        --debug) ydb_debug="Y" ; set -x ; shift ;;
        --distrib*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then ydb_distrib=$tmp
            else if [ 1 -lt "$#" ] ; then ydb_distrib=$2 ; shift
                else echo "--distrib needs a value" ; err_exit
                fi
            fi
            shift ;;
        --dry-run) gtm_dryrun="Y" ; shift ;;
        --gtm)
            gtm_gtm="Y"
            shift ;;
        --group-restriction) gtm_group_restriction="Y" ; shift ;; # must come before group*
        --group*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then gtm_group=$tmp
            else if [ 1 -lt "$#" ] ; then gtm_group=$2 ; shift
                else echo "--group needs a value" ; err_exit
                fi
            fi
            shift ;;
        --help) help_exit ;;
        --installdir*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then ydb_installdir=$tmp
            else if [ 1 -lt "$#" ] ; then ydb_installdir=$2 ; shift
                else echo "--installdir needs a value" ; err_exit
                fi
            fi
            shift ;;
        --keep-obj) gtm_keep_obj="Y" ; shift ;;
        --linkenv*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then gtm_linkenv=$tmp
            else if [ 1 -lt "$#" ] ; then gtm_linkenv=$2 ; shift
                else echo "--linkenv needs a value" ; err_exit
                fi
            fi
            unset gtm_copyenv
            shift ;;
        --linkexec*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then gtm_linkexec=$tmp
            else if [ 1 -lt "$#" ] ; then gtm_linkexec=$2 ; shift
                else echo "--linkexec needs a value" ; err_exit
                fi
            fi
            unset gtm_copyexec
            shift ;;
        --overwrite-existing) gtm_overwrite_existing="Y" ; shift ;;
        --preserveRemoveIPC) ydb_change_removeipc="no" ; shift ;; # must come before group*
        --prompt-for-group) gtm_prompt_for_group="Y" ; shift ;;
        --ucaseonly-utils) gtm_lcase_utils="N" ; shift ;;
        --user*) tmp=`echo $1 | cut -s -d = -f 2-`
            if [ -n "$tmp" ] ; then gtm_user=$tmp
            else if [ 1 -lt "$#" ] ; then gtm_user=$2 ; shift
                else echo "--user needs a value" ; err_exit
                fi
            fi
            shift ;;
        --utf8*) tmp=`echo $1 | cut -s -d = -f 2- | tr DEFAULT default`
            if [ -n "$tmp" ] ; then gtm_icu_version=$tmp
            else if [ 1 -lt "$#" ] ; then gtm_icu_version=`echo $2 | tr DEFAULT default`; shift
                else echo "--utf8 needs a value" ; err_exit
                fi
            fi
            shift ;;
        --verbose) gtm_verbose="Y" ; shift ;;
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
    aix*) # no Source Forge dirname
        gtm_arch="rs6000" # uname -m is not useful on AIX
        gtm_ftp_dirname="aix"
        ydb_flavor="rs6000"
        gtm_install_flavor="RS6000" ;;
    hpux_ia64) # no Source Forge dirname
        gtm_ftp_dirname="hpux_ia64"
        ydb_flavor="ia64"
        gtm_install_flavor="IA64" ;;
    linux_i586)
        gtm_sf_dirname="GT.M-x86-Linux"
        gtm_ftp_dirname="linux"
        ydb_flavor="i586"
        gtm_install_flavor="x86"
        gtm_shlib_support="N" ;;
    linux_i686)
        gtm_sf_dirname="GT.M-x86-Linux"
        gtm_ftp_dirname="linux"
        ydb_flavor="i686"
        gtm_install_flavor="x86"
        gtm_shlib_support="N" ;;
    linux_ia64) # no Source Forge dirname
        gtm_ftp_dirname="linux_ia64"
        ydb_flavor="ia64"
        gtm_install_flavor="IA" ;;
    linux_s390x) # no Source Forge dirname
        gtm_ftp_dirname="linux_s390x"
        ydb_flavor="s390x"
        gtm_install_flavor="S390X" ;;
    linux_x8664)
        gtm_sf_dirname="GT.M-amd64-Linux"
        gtm_ftp_dirname="linux_x8664"
        ydb_flavor="x8664"
        gtm_install_flavor="x86_64" ;;
    linux_armv6l)
        ydb_flavor="armv6l" ;;
    linux_armv7l)
        ydb_flavor="armv7l" ;;
    solaris_sparc) # no Source Forge dirname
        gtm_ftp_dirname="sun"
        ydb_flavor="sparc"
        gtm_install_flavor="SPARC" ;;
    *) echo Architecture `uname -o` on `uname -m` not supported by this script ; err_exit ;;
esac

# YottaDB version is required - first see if ydbinstall and mumps are bundled
if [ -z "$ydb_version" ] ; then
    tmp=`dirname $0`
    if [ -e "$tmp/mumps" -a -e "$tmp/_XCMD.m" ] ; then
        ydb_distrib=$tmp
        ydb_dist=$tmp ; export ydb_dist
        chmod +x $ydb_dist/mumps
        tmp=`mktmpdir`
        gtmroutines="$tmp($ydb_dist)" ; export gtmroutines
        ydb_version=`$ydb_dist/mumps -run %XCMD 'write $piece($zyrelease," ",2)'`
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
    ydb_distrib="https://api.github.com/repos/YottaDB/YottaDB/"
fi
if [ "Y" = "$gtm_gtm" ] ; then
    ydb_distrib="http://sourceforge.net/projects/fis-gtm"
fi

tmpdir=`mktmpdir`
gtm_tmp=$tmpdir
mkdir $gtm_tmp/tmp
latest=`echo "$ydb_version" | tr LATES lates`
if [ -z "$ydb_version" -o "latest" = "$latest" ] ; then
    case $ydb_distrib in
        http://sourceforge.net/projects/fis-gtm | https://sourceforge.net/projects/fis-gtm)
            gtm_gtm="Y"
            if [ "Y" = "$gtm_verbose" ] ; then
               echo wget ${ydb_distrib}/files/${gtm_sf_dirname}/latest to determine latest version
               echo Check proxy settings if wget hangs
            fi
            if { wget $wget_flags $gtm_tmp ${ydb_distrib}/files/${gtm_sf_dirname}/latest 2>&1 1>${gtm_tmp}/wget_latest.log ; } ; then
                ydb_version=`cat ${gtm_tmp}/latest`
            else echo Unable to determine YottaDB/GT.M version ; err_exit
            fi ;;
        ftp://*)
            if [ "Y" = "$gtm_verbose" ] ; then
               echo wget $gtm_tmp ${ydb_distrib}/${gtm_ftp_dirname}/latest to determine latest version
               echo Check proxy settings if wget hangs
            fi
            if { wget $wget_flags $gtm_tmp ${ydb_distrib}/${gtm_ftp_dirname}/latest 2>&1 1>${gtm_tmp}/wget_latest.log ; } ; then
                ydb_version=`cat ${gtm_tmp}/latest`
            else echo Unable to determine YottaDB/GT.M version ; err_exit
            fi ;;
        https://api.github.com/repos/YottaDB/YottaDB* | https://github.com/YottaDB/YottaDB*)
            if [ "Y" = "$gtm_verbose" ] ; then
                echo wget https://api.github.com/repos/YottaDB/YottaDB/releases/latest to determine latest version
                echo Check proxy settings if wget hangs
            fi
            if { wget $wget_flags $gtm_tmp https://api.github.com/repos/YottaDB/YottaDB/releases/latest 2>&1 1>${gtm_tmp}/wget_latest.log ; } ; then
                ydb_version=`grep "tag_name" ${gtm_tmp}/latest | cut -d'"' -f4`
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
if [ -f "${ydb_distrib}/mumps" ] ; then gtm_tmp=$ydb_distrib
else
    tmp=`echo $ydb_version | tr -d .-`
    ydb_filename=""
    if [ "Y" = "$gtm_gtm" ] ; then ydb_filename=gtm_${tmp}_${gtm_hostos}_${ydb_flavor}_${gtm_buildtype}.tar.gz ; fi
    case $ydb_distrib in
        http://sourceforge.net/projects/fis-gtm | https://sourceforge.net/projects/fis-gtm)
            if [ "Y" = "$gtm_verbose" ] ; then
                echo wget ${ydb_distrib}/files/${gtm_sf_dirname}/${ydb_version}/${ydb_filename} to download tarball
                echo Check proxy settings if wget hangs
            fi
            if { ! wget $wget_flags $gtm_tmp ${ydb_distrib}/files/${gtm_sf_dirname}/${ydb_version}/${ydb_filename} \
                	2>&1 1>${gtm_tmp}/wget_dist.log ; } ; then
                echo Unable to download GT.M distribution $ydb_filename ; err_exit
            fi ;;
        https://api.github.com/repos/YottaDB/YottaDB* | https://github.com/YottaDB/YottaDB*)
            if [ "Y" = "$gtm_verbose" ] ; then
                echo wget https://api.github.com/repos/YottaDB/YottaDB/releases/tags/${ydb_version} and parse to download tarball
                echo Check proxy settings if wget hangs
            fi
            if { wget $wget_flags $gtm_tmp https://api.github.com/repos/YottaDB/YottaDB/releases/tags/${ydb_version} 2>&1 1>${gtm_tmp}/wget_dist.log ;} ; then
		# There might be multiple binary tarballs of YottaDB (for various architectures & platforms).
		# If so, choose the one that corresponds to the current host.
		yottadb_download_urls=`grep "browser_download_url" ${gtm_tmp}/${ydb_version} | cut -d'"' -f4`
		# Determine current host's architecture
		arch=`uname -m | tr -d '_'`
		# Determine current host's OS. We expect the OS name in the tarball.
		platform=`uname -s | tr '[A-Z]' '[a-z]'`
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
			if [ $platform = "linux" ] ; then
				# If the current host is a RHEL box then set the OS to RHEL (not Linux) as there is a RHEL-specific tarball.
				# We check that it is a RHEL box by the existence of the file /etc/redhat-release.
				# But note that RHEL-specific releases of YottaDB happened only starting r1.10 so do this only
				# if the requested version is not r1.00 (the only YottaDB release prior to r1.10)
				if [ -e /etc/redhat-release -a "r1.00" != ${ydb_version} ] ; then
					platform="rhel"
				fi
			fi
			case $ydb_filename in
				*"$platform"*) ;;			# If tarball has current architecture in its name, consider it
				*)             continue ;;		# else move on to next tarball
			esac
			yottadb_download_url=$fullfilename
			break					# Now that we found one tarball, stop looking at other choices
		done
		if [ $yottadb_download_url = "" ]; then echo Unable to find YottaDB tarball for ${ydb_version} $platform $arch ; err_exit; fi
                wget $wget_flags $gtm_tmp $yottadb_download_url
                if [ ! -f ${gtm_tmp}/${ydb_filename} ]; then echo Unable to download YottaDB distribution $ydb_filename ; err_exit; fi
            else echo Error during wget of YottaDB distribution file ${ydb_distrib}/${ydb_filename} ; err_exit
            fi ;;
        ftp://*)
            if [ "Y" = "$gtm_verbose" ] ; then
                echo wget ${ydb_distrib}/${gtm_ftp_dirname}/${tmp}/${ydb_filename} to download tarball
                echo Check proxy settings if wget hangs
            fi
            if { ! wget $wget_flags $gtm_tmp ${ydb_distrib}/${gtm_ftp_dirname}/${tmp}/${ydb_filename} \
                	2>&1 1>${gtm_tmp}/wget_dist.log ; } ; then
                echo Unable to download GT.M distribution $ydb_filename ; err_exit
            fi ;;
        *)
            if [ -f ${ydb_distrib}/${ydb_filename} ] ; then
                if [ "Y" = "$gtm_verbose" ] ; then echo tarball is ${ydb_distrib}/${ydb_filename} ; fi
                ln -s ${ydb_distrib}/${ydb_filename} $gtm_tmp
            else echo Unable to locate YottaDB/GT.M distribution file ${ydb_distrib}/${ydb_filename} ; err_exit
            fi ;;
    esac
    ( cd $gtm_tmp/tmp ; gzip -d < ${gtm_tmp}/${ydb_filename} | tar xf - 2>&1 1>${gtm_tmp}/tar.log )
fi
if [ "Y" = "$gtm_verbose" ] ; then echo Downloaded and unpacked YottaDB/GT.M distribution ; dump_info ; fi

# Check installation settings & provide defaults as needed
tmp=`$ydb_id -un`
if [ -z "$gtm_user" ] ; then gtm_user=$tmp
else if [ "$gtm_user" != "`$ydb_id -un $gtm_user`" ] ; then
    echo $gtm_user is a non-existent user ; err_exit
    fi
fi
if [ "root" = $tmp ] ; then
    if [ -z "$gtm_group" ] ; then gtm_group=`$ydb_id -gn`
    else if [ "root" != "$gtm_user" -a "$gtm_group" != "`$ydb_id -Gn $gtm_user | xargs -n 1 | $ydb_grep $gtm_group`" ] ; then
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
if [ -d "$ydb_installdir" -a "Y" != "$gtm_overwrite_existing" ] ; then
    echo $ydb_installdir exists and --overwrite-existing not specified ; err_exit
fi

if [ "Y" = "$gtm_verbose" ] ; then echo Finished checking options and assigning defaults ; dump_info ; fi

# Prepare input to YottaDB configure script. The corresponding questions in configure.gtc are listed below in comments
gtm_configure_in=${gtm_tmp}/configure_${timestamp}.in
export ydb_change_removeipc			# Signal configure.gtc to set RemoveIPC=no or not, if needed
echo $gtm_user >>$gtm_configure_in		# Response to : "What user account should own the files?"
echo $gtm_group >>$gtm_configure_in		# Response to : "What group should own the files?"
echo $gtm_group_restriction >>$gtm_configure_in	# Response to : "Should execution of YottaDB be restricted to this group?"
echo $ydb_installdir >>$gtm_configure_in	# Response to : "In what directory should YottaDB be installed?"
echo y >>$gtm_configure_in			# Response to one of two possible questions
						#	"Directory $ydb_dist exists. If you proceed with this installation then some files will be over-written. Is it ok to proceed?"
						#	"Directory $ydb_dist does not exist. Do you wish to create it as part of this installation? (y or n)"
if [ -z "$gtm_icu_version" ] ; then echo n  >>$gtm_configure_in	# Response to : "Should UTF-8 support be installed?"
else echo y  >>$gtm_configure_in		# Response to : "Should UTF-8 support be installed?"
    if [ "default" = $gtm_icu_version ] ; then echo n  >>$gtm_configure_in	# Response to : "Should an ICU version other than the default be used?"
    else echo y >>$gtm_configure_in		# Response to : "Should an ICU version other than the default be used?"
        echo $gtm_icu_version >>$gtm_configure_in	# Response to : "Enter ICU version"
    fi
fi
echo $gtm_lcase_utils >>$gtm_configure_in	# Response to : "Do you want uppercase and lowercase versions of the MUMPS routines?"
if [ "Y" = $gtm_shlib_support ] ; then echo $gtm_keep_obj >>$gtm_configure_in ; fi	# Response to : "Object files of M routines placed in shared library $ydb_dist/libyottadbutil$ext. Keep original .o object files (y or n)?"
echo n >>$gtm_configure_in			# Response to : "Installation completed. Would you like all the temporary files removed from this directory?"
if [ "Y" = "$gtm_verbose" ] ; then echo Prepared configuration file ; cat $gtm_configure_in ; dump_info ; fi


# Run the YottaDB configure script
if [ "$ydb_distrib" != "$gtm_tmp" ] ; then
    chmod +w $gtm_tmp/tmp
    cd $gtm_tmp/tmp
    # Starting YottaDB r1.10, unpacking the binary tarball creates an additional directory (e.g. yottadb_r110)
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
if [ "Y" = "$gtm_dryrun" ] ; then echo Installation prepared in $gtm_tmp ; exit ; fi

./configure.sh <$gtm_configure_in 1> $gtm_tmp/configure_${timestamp}.out 2>$gtm_tmp/configure_${timestamp}.err
if [ $? -gt 0 ] ; then echo "configure.sh failed. Output follows"; cat $gtm_tmp/configure_${timestamp}.out $gtm_tmp/configure_${timestamp}.err ; exit 1; fi
rm -rf $tmpdir	# Now that install is successful, remove temporary directory
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
