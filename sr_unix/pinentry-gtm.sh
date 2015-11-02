#!/bin/sh
#################################################################
#                                                               #
#       Copyright 2010 Fidelity Information Services, Inc #
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################

#############################################################################################
#
#       pinentry-gtm.sh - This script is used as a "pinentry-program" in gpg-agent.conf.
#	If the gtm_passwd environment variable exists and a usable mumps exists, run
#	pinentry.m to get the passphrase from the environment variable.
#
#############################################################################################

dir=`dirname $0` ; if [ -z "$dir" ] ; then dir=$PWD ; fi
if [ -z "$gtm_dist" ] ; then
    if [ "UTF-8" = "$gtm_chset" -a -x "$dir/../../utf8/mumps" ] ; then
	export gtm_dist=$dir/../../utf8
    elif [ -x "$dir/../../mumps" ] ; then export gtm_dist=$dir/../..
    fi
fi

if [ -n "$gtm_passwd" -a -x "$gtm_dist/mumps" ] ; then

    # temporary directory for object routines
    if [ -x "`which mktemp 2>/dev/null`" ] ; then
	 tmpdir=`mktemp -d`
    else
	tmpdir=/tmp/`basename $0`_$$.tmp ; mkdir $tmpdir
    fi

    trap 'rm -f $tmpdir ; exit 1' HUP INT QUIT TERM TRAP
    gtmroutines="$tmpdir($dir) $gtm_dist" $gtm_dist/mumps -run pinentry
    rm -rf $tmpdir

else # punt to the regular pinentry program
    pinentry=`which pinentry 2>/dev/null`
    if [ -x "$pinentry" ] ; then $pinentry $* ; else exit 1 ; fi
fi
