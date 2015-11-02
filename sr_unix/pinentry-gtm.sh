#!/bin/sh
#################################################################
#                                                               #
#       Copyright 2010, 2011 Fidelity Information Services, Inc #
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

# Obfuscated password is obtained by a combination of the password, $USER and the inode of $gtm_dist/mumps. If $gtm_chset is set to
# UTF-8, the resulting character stream need not always represent valid unicode code points when read by pinentry.m. To work around
# this, force gtm_chset to M if we are coming in with gtm_chset set to UTF-8 and restore it before exit.
if [ "UTF-8" = "$gtm_chset" ] ; then
	save_gtm_chset=$gtm_chset
	save_gtmroutines=$gtmroutines
fi

# Pinentry M program is invoked whenever GT.M/MUPIP needs the clear-text password to encrypt/decrypt the database. But, it does so
# while holding database startup locks. If Pinentry itself ended up doing database access, we could create a deadlock because
# Pinentry will need database startup locks which is held by GT.M/MUPIP and the latter won't let go of the locks until Pinentry
# exits. Although Pinentry doesn't do explicit database access, it could indirectly end up accessing the database if
# $gtm_trace_gbl_name is set in the environment. So, temporarily set this environment variable to an empty string before invoking
# Pinentry. Since the caller of the script could potentially source this script, save it to a temporary variable and restore it
# before exit.
save_gtm_trace_gbl_name=$gtm_trace_gbl_name
export gtm_trace_gbl_name=""

if [ -z "$gtm_dist" ] ; then
	# $gtm_dist is not set in the environment. See if we can use dirname to find one
	if [ -x "$dir/../../mumps" ] ; then export gtm_dist=$dir/../.. ; fi
fi

if [ -n "$gtm_passwd" -a -x "$gtm_dist/mumps" ] ; then
	# temporary directory for object routines
	if [ -x "`which mktemp 2>/dev/null`" ] ; then
		tmpdir=`mktemp -d`
	else
		tmpdir=/tmp/`basename $0`_$$.tmp ; mkdir $tmpdir
	fi
	trapstr="rm -f $tmpdir ; gtm_chset=$save_gtm_chset ; gtmroutines=$save_gtmroutines"
	trapstr="$trapstr ; gtm_trace_gbl_name=$save_gtm_trace_gbl_name"
	trap "$trapstr" HUP INT QUIT TERM TRAP
	gtm_chset="M"
	gtmroutines="$tmpdir($dir) $gtm_dist"
	$gtm_dist/mumps -run pinentry
	rm -rf $tmpdir
else # punt to the regular pinentry program
	pinentry=`which pinentry 2>/dev/null`
	if [ -x "$pinentry" ] ; then $pinentry $* ; else exit 1 ; fi
fi
# Now that we are done with fetching the obfuscated password, restore gtm_chset and gtmroutines to their values noted down at
# function entry. Also, restore gtm_trace_gbl_name variable to the value noted down at function entry.
gtm_chset=$save_gtm_chset
gtmroutines=$save_gtmroutines
gtm_trace_gbl_name=$save_gtm_trace_gbl_name
