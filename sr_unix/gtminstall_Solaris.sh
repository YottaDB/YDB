#!/usr/xpg4/bin/sh -
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

# This script automates the installation of GT.M as much as possible,
# to the extent of attempting to download the distribution file.
# Current limitation is GNU/Linux on x86 (32- & 64-bit) architectures
# and root installation, but it is intended to relax this in the future.

# NOTE: This script requires the GNU Wget program to download
# distribution files that are not on the local file system.

# CAUTION - this script is still experimental and unstable.
# z/OS, HP-UX on PA-RISC and Tru64 UNIX are not supported.

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

# Turn on debugging if set
if [ "Y" = "$gtm_debug" ] ; then set -x ; fi

# Initialization
timestamp=`date +%Y%m%d%H%M%S`
if [ -x "/usr/bin/which" ] ; then which=/usr/bin/which ; else which=which ; fi
if [ "SunOS" = `uname -s` ] ; then
    gtm_id="/usr/xpg4/bin/id"
    gtm_grep="/usr/xpg4/bin/grep"
else
    gtm_id=`which id`
    gtm_grep=`which grep`
fi
if [ -z "$USER" ] ; then USER=`$gtm_id -un` ; fi

# Functions
dump_info()
{
    set +x
    if [ -n "$gtm_arch" ] ; then echo gtm_arch " : " $gtm_arch ; fi
    if [ -n "$gtm_buildtype" ] ; then echo gtm_buildtype " : " $gtm_buildtype ; fi
    if [ -n "$gtm_configure_in" ] ; then echo gtm_configure_in " : " $gtm_configure_in ; fi
    if [ -n "$gtm_copyenv" ] ; then echo gtm_copyenv " : " $gtm_copyenv ; fi
    if [ -n "$gtm_copyexec" ] ; then echo gtm_copyexec " : " $gtm_copyexec ; fi
    if [ -n "$gtm_debug" ] ; then echo gtm_debug " : " $gtm_debug ; fi
    if [ -n "$gtm_dist" ] ; then echo gtm_dist " : " $gtm_dist ; fi
    if [ -n "$gtm_distrib" ] ; then echo gtm_distrib " : " $gtm_distrib ; fi
    if [ -n "$gtm_dryrun" ] ; then echo gtm_dryrun " : " $gtm_dryrun ; fi
    if [ -n "$gtm_filename" ] ; then echo gtm_filename " : " $gtm_filename ; fi
    if [ -n "$gtm_flavor" ] ; then echo gtm_flavor " : " $gtm_flavor ; fi
    if [ -n "$gtm_ftp_dirname" ] ; then echo gtm_ftp_dirname " : " $gtm_ftp_dirname ; fi
    if [ -n "$gtm_group" ] ; then echo gtm_group " : " $gtm_group ; fi
    if [ -n "$gtm_group_already" ] ; then echo gtm_group_already " : " $gtm_group_already ; fi
    if [ -n "$gtm_group_restriction" ] ; then echo gtm_group_restriction " : " $gtm_group_restriction ; fi
    if [ -n "$gtm_hostos" ] ; then echo gtm_hostos " : " $gtm_hostos ; fi
    if [ -n "$gtm_icu_version" ] ; then echo gtm_icu_version " : " $gtm_icu_version ; fi
    if [ -n "$gtm_install_flavor" ] ; then echo gtm_install_flavor " : " $gtm_install_flavor ; fi
    if [ -n "$gtm_installdir" ] ; then echo gtm_installdir " : " $gtm_installdir ; fi
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
    if [ -n "$gtm_version" ] ; then echo gtm_version " : " $gtm_version ; fi
    if [ -n "$gtmroutines" ] ; then echo gtmroutines " : " $gtmroutines ; fi
    if [ -n "$timestamp" ] ; then echo timestamp " : " $timestamp ; fi
    if [ "Y" = "$gtm_debug" ] ; then set -x ; fi
}

err_exit()
{
    set +x
    echo "gtminstall [option] ... [version]"
    echo "Options are:"
    echo "--build-type buildtype - * type of GT.M build, default is pro"
    echo "--copyenv dirname - copy gtmprofile and gtmcshrc files to dirname; incompatible with linkenv"
    echo "--copyexec dirname - copy gtm script to dirname; incompatible with linkexec"
    echo "--debug - * turn on debugging with set -x"
    echo "--distrib dirname or URL - source directory for GT.M distribution tarball, local or remote"
    echo "--dry-run - do everything short of installing GT.M, including downloading the distribution"
    echo "--group group - group that should own the GT.M installation"
    echo "--group-restriction - limit execution to a group; defaults to unlimited if not specified"
    echo "--help - print this usage information"
    echo "--installdir dirname - directory where GT.M is to be installed; defaults to /usr/lib/fis-gtm/version_platform"
    m1="--keep-obj - keep .o files"
    m1="$m1"" of M routines (normally deleted on platforms with GT.M support for routines in shared libraries)"
    echo "$m1"
    echo "--linkenv dirname - create link in dirname to gtmprofile and gtmcshrc files; incompatible with copyenv"
    echo "--linkexec dirname - create link in dirname to gtm script; incompatible with copyexec"
    echo "--overwrite-existing - install into an existing directory, overwriting contents; defaults to requiring new directory"
    m1="--prompt-for-group - * GT.M installation "
    m1="$m1""script will prompt for group; default is yes for production releases V5.4-002 or later, no for all others"
    echo "$m1"
    echo "--ucaseonly-utils -- install only upper case utility program names; defaults to both if not specified"
    echo "--user username - user who should own GT.M installation; default is root"
    m1="--utf8 ICU_version - install "
    m1="$m1""UTF-8 support using specified  major.minor ICU version; specify default to use default version"
    echo "$m1"
    echo "--verbose - * output diagnostic information as the script executes; default is to run quietly"
    echo "options that take a value (e.g, --group) can be specified as either --option=value or --option value"
    echo "options marked with * are likely to be of interest primarily to GT.M developers"
    echo "version is defaulted from mumps file if one exists in the same directory as the installer"
    echo "This version must run as root."
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
if [ -z "$gtm_lcase_utils" ] ; then gtm_lcase_utils="Y" ; fi
if [ -z "$gtm_overwrite_existing" ] ; then gtm_overwrite_existing="N" ; fi
if [ -z "$gtm_prompt_for_group" ] ; then gtm_prompt_for_group="N" ; fi
if [ -z "$gtm_verbose" ] ; then gtm_verbose="N" ; fi

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
	--debug) gtm_debug="Y" ; set -x ; shift ;;
	--distrib*) tmp=`echo $1 | cut -s -d = -f 2-`
	    if [ -n "$tmp" ] ; then gtm_distrib=$tmp
	    else if [ 1 -lt "$#" ] ; then gtm_distrib=$2 ; shift
    	        else echo "--distrib needs a value" ; err_exit
    	        fi
	    fi
	    shift ;;
	--dry-run) gtm_dryrun="Y" ; shift ;;
	--group-restriction) gtm_group_restriction="Y" ; shift ;; # must come before group*
	--group*) tmp=`echo $1 | cut -s -d = -f 2-`
	    if [ -n "$tmp" ] ; then gtm_group=$tmp
	    else if [ 1 -lt "$#" ] ; then gtm_group=$2 ; shift
    	        else echo "--group needs a value" ; err_exit
    	        fi
	    fi
	    shift ;;
	--help) err_exit ;;
	--installdir*) tmp=`echo $1 | cut -s -d = -f 2-`
	    if [ -n "$tmp" ] ; then gtm_installdir=$tmp
	    else if [ 1 -lt "$#" ] ; then gtm_installdir=$2 ; shift
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
	*) if [ -n "$gtm_version" ] ; then echo Nothing must follow the GT.M version ; err_exit
	    else gtm_version=$1 ; shift ; fi
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
	gtm_flavor="rs6000"
	gtm_install_flavor="RS6000" ;;
    hpux_ia64) # no Source Forge dirname
	gtm_ftp_dirname="hpux_ia64"
	gtm_flavor="ia64"
	gtm_install_flavor="IA64" ;;
    linux_i686) gtm_sf_dirname="GT.M-x86-Linux"
	gtm_ftp_dirname="linux"
	gtm_flavor="i686"
	gtm_install_flavor="x86"
	gtm_shlib_support="N" ;;
    linux_ia64) # no Source Forge dirname
	gtm_ftp_dirname="linux_ia64"
	gtm_flavor="ia64"
	gtm_install_flavor="IA" ;;
    linux_s390x) # no Source Forge dirname
	gtm_ftp_dirname="linux_s390x"
	gtm_flavor="s390x"
	gtm_install_flavor="S390X" ;;
    linux_x8664) gtm_sf_dirname="GT.M-amd64-Linux"
	gtm_ftp_dirname="linux_x8664"
	gtm_flavor="x8664"
	gtm_install_flavor="x86_64" ;;
    solaris_sparc) # no Source Forge dirname
	gtm_ftp_dirname="sun"
	gtm_flavor="sparc"
	gtm_install_flavor="SPARC" ;;
    default) echo Architecture `uname -o` on `uname -m` not supported by this script ; err_exit ;;
esac

# GT.M version is required - first see if gtminstall and mumps are bundled
if [ -z "$gtm_version" ] ; then
    tmp=`dirname $0`
    if [ -e "$tmp/mumps" -a -e "$tmp/_XCMD.m" ] ; then
	gtm_distrib=$tmp
	gtm_dist=$tmp ; export gtm_dist
	chmod +x $gtm_dist/mumps
	tmp=`mktmpdir`
	gtmroutines="$tmp($gtm_dist)" ; export gtmroutines
	gtm_version=`$gtm_dist/mumps -run %XCMD 'write $piece($zversion," ",2)'`
	rm -rf $tmp
    fi
fi
if [ "Y" = "$gtm_verbose" ] ; then echo Determined architecture, OS and GT.M version ; dump_info ; fi

# See if GT.M version can be determined from meta data
if [ -z "$gtm_distrib" ] ; then
    gtm_distrib=http://sourceforge.net/projects/fis-gtm
fi
gtm_tmp=`mktmpdir`
mkdir $gtm_tmp/tmp
if [ -z "$gtm_version" -o "latest" = "`echo "$gtm_version" | tr LATES lates`" ] ; then
    case $gtm_distrib in
	http://sourceforge.net/projects/fis-gtm | https://sourceforge.net/projects/fis-gtm)
	    if { wget -qP $gtm_tmp ${gtm_distrib}/files/${gtm_sf_dirname}/latest 2>&1 1>${gtm_tmp}/wget_latest.log ; } ; then
		gtm_version=`cat ${gtm_tmp}/latest`
	    else echo Unable to determine GT.M version ; err_exit
	    fi ;;
	ftp://*)
	    if { wget -qP $gtm_tmp ${gtm_distrib}/${gtm_ftp_dirname}/latest 2>&1 1>${gtm_tmp}/wget_latest.log ; } ; then
		gtm_version=`cat ${gtm_tmp}/latest`
	    else echo Unable to determine GT.M version ; err_exit
	    fi ;;
	*) if [ -f ${gtm_distrib}/latest ] ; then gtm_version=`cat ${gtm_distrib}/latest`
	    else echo Unable to determine GT.M version ; err_exit
	    fi ;;
    esac
fi
if [ -z "$gtm_version" ] ; then
echo GT.M version to install is required ; err_exit
fi

# Get GT.M distribution if gtminstall is not bundled with distribution
if [ -f "${gtm_distrib}/mumps" ] ; then gtm_tmp=$gtm_distrib
else
    tmp=`echo $gtm_version | tr -d .-`
    gtm_filename=gtm_${tmp}_${gtm_hostos}_${gtm_flavor}_${gtm_buildtype}.tar.gz
    case $gtm_distrib in
	http://sourceforge.net/projects/fis-gtm | https://sourceforge.net/projects/fis-gtm)
	    if { ! wget -qP $gtm_tmp ${gtm_distrib}/files/${gtm_sf_dirname}/${gtm_version}/${gtm_filename} \
	        2>&1 1>${gtm_tmp}/wget_dist.log ; } ; then
		echo Unable to download GT.M distribution $gtm_filename ; err_exit
	    fi ;;
	ftp://*)
	    if { ! wget -qP $gtm_tmp ${gtm_distrib}/${gtm_ftp_dirname}/${tmp}/${gtm_filename} \
	        2>&1 1>${gtm_tmp}/wget_dist.log ; } ; then
		echo Unable to download GT.M distribution $gtm_filename ; err_exit
	    fi ;;
	*) if [ -f ${gtm_distrib}/${gtm_filename} ] ; then ln -s ${gtm_distrib}/${gtm_filename} $gtm_tmp
	    else echo Unable to locate GT.M distribution file ${gtm_distrib}/${gtm_filename} ; err_exit
	    fi ;;
    esac
    ( cd $gtm_tmp/tmp ; gzip -d < ${gtm_tmp}/${gtm_filename} | tar xf - 2>&1 1>${gtm_tmp}/tar.log )
fi
if [ "Y" = "$gtm_verbose" ] ; then echo Downloaded and unpacked GT.M distribution ; dump_info ; fi

# Check installation settings & provide defaults as needed
tmp=`$gtm_id -un`
if [ -z "$gtm_user" ] ; then gtm_user=$tmp
else if [ "$gtm_user" != "`$gtm_id -un $gtm_user`" ] ; then
    echo $gtm_user is a non-existent user ; err_exit
    fi
fi
if [ "root" = $tmp ] ; then
    if [ -z "$gtm_group" ] ; then gtm_group=`$gtm_id -gn`
    else if [ "root" != "$gtm_user" -a "$gtm_group" != "`$gtm_id -Gn $gtm_user | xargs -n 1 | $gtm_grep $gtm_group`" ] ; then
	echo $gtm_user is not a member of $gtm_group ; err_exit
        fi
    fi
 else
    echo Non-root installations not currently supported
    if [ "N" = "$gtm_dryrun" ] ; then err_exit
    else echo "Continuing because --dry-run selected"
    fi
fi
if [ -z "$gtm_installdir" ] ; then gtm_installdir=/usr/lib/fis-gtm/${gtm_version}_${gtm_install_flavor} ; fi
if [ -d "$gtm_installdir" -a "Y" != "$gtm_overwrite_existing" ] ; then
    echo $gtm_installdir exists and --overwrite-existing not specified ; err_exit
fi
if [ "Y" = "$gtm_verbose" ] ; then echo Finished checking options and assigning defaults ; dump_info ; fi

# Prepare input to GT.M configure script
gtm_configure_in=${gtm_tmp}/configure_${timestamp}.in
if { ! $gtm_id -gn bin 2>/dev/null 1>/dev/null ; } then
    if [ "N" = "$gtm_prompt_for_group" -o 54002 -gt `echo $gtm_version | cut -s -d V -f 2- | tr -d A-Za-z.-` ] ; then
	echo y >>$gtm_configure_in
	echo root >>$gtm_configure_in
	echo $gtm_group_restriction >>$gtm_configure_in
	gtm_group_already="Y"
    fi
fi
echo $gtm_user >>$gtm_configure_in
if [ "Y" = "$gtm_prompt_for_group" -o 54002 -le `echo $gtm_version | cut -s -d V -f 2- | tr -d A-Za-z.-` ] ; then
    echo $gtm_group >>$gtm_configure_in
fi
if [ "N" = "$gtm_group_already" ] ; then
    echo $gtm_group_restriction >>$gtm_configure_in
    if [ "Y" = "$gtm_group_restriction" ] ; then echo $gtm_group >>$gtm_configure_in ; fi
fi
echo $gtm_installdir >>$gtm_configure_in
echo y >>$gtm_configure_in
if [ -z "$gtm_icu_version" ] ; then echo n  >>$gtm_configure_in
else echo y  >>$gtm_configure_in
    if [ "default" = $gtm_icu_version ] ; then echo n  >>$gtm_configure_in
    else echo y >>$gtm_configure_in
	echo $gtm_icu_version >>$gtm_configure_in
    fi
fi
echo $gtm_lcase_utils >>$gtm_configure_in
if [ "Y" = $gtm_shlib_support ] ; then echo $gtm_keep_obj >>$gtm_configure_in ; fi
echo n >>$gtm_configure_in
if [ "Y" = "$gtm_verbose" ] ; then echo Prepared configuration file ; cat $gtm_configure_in ; dump_info ; fi


# Run the GT.M configure script
if [ "$gtm_distrib" != "$gtm_tmp" ] ; then
    chmod +w $gtm_tmp/tmp
    cd $gtm_tmp/tmp
fi
tmp=`head -1 configure | cut -f 1`
if [ "#!/bin/sh" != "$tmp" ] ; then
    echo "#!/bin/sh" >configure.sh
    cat configure >>configure.sh
else
    cp configure configure.sh
fi
chmod +x configure.sh

# Stop here if this is a dry run
if [ "Y" = "$gtm_dryrun" ] ; then echo Installation prepared in $gtm_tmp ; exit ; fi

./configure.sh <$gtm_configure_in 1> $gtm_tmp/configure_${timestamp}.out 2>$gtm_tmp/configure_${timestamp}.err
if [ "Y" = "$gtm_verbose" ] ; then echo Installation complete ; ls -l $gtm_installdir ; fi

# Create copies of environment scripts and gtm executable
if [ -d "$gtm_linkenv" ] ; then
    ( cd $gtm_linkenv ; ln -s $gtm_installdir/gtmprofile $gtm_installdir/gtmcshrc ./ )
    if [ "Y" = "$gtm_verbose" ] ; then echo Linked env ; ls -l $gtm_linkenv ; fi
else if [ -d "$gtm_copyenv" ] ; then
        ( cd $gtm_linkenv ; cp $gtm_installdir/gtmprofile $gtm_installdir/gtmcshrc ./ )
	if [ "Y" = "$gtm_verbose" ] ; then echo Copied env ; ls -l $gtm_copyenv ; fi
     fi
fi
if [ -d "$gtm_linkexec" ] ; then
    ( cd $gtm_linkexec ; ln -s $gtm_installdir/gtm ./ )
    if [ "Y" = "$gtm_verbose" ] ; then echo Linked exec ; ls -l $gtm_linkexec ; fi
else if [ -d "$gtm_copyexec" ] ; then
        ( cd $gtm_linkexec ; cp $gtm_installdir/gtm ./ )
	if [ "Y" = "$gtm_verbose" ] ; then echo Copied exec ; ls -l $gtm_copyexec ; fi
     fi
fi
