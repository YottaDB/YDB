#!/bin/sh
#################################################################
#                                                               #
# Copyright (c) 2010-2015 Fidelity National Information 	#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
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
punt=1

if [ -z "$gtm_dist" ] ; then
	# $gtm_dist is not set in the environment. See if we can use dirname to find one
	if [ "`echo $gtm_chset | tr utf UTF`" = "UTF-8" -a -x "$dir/../../utf8/mumps" ] ; then
		export gtm_dist=$dir/../../utf8
	elif [ -x "$dir/../../mumps" ] ; then
		export gtm_dist=$dir/../..
		unset gtm_chset
	fi
fi

if [ -n "$gtm_passwd" -a -x "$gtm_dist/mumps" ] ; then
	# Password and MUMPS exists, perform some extended setup checks
	if [ -z "$gtmroutines" ] ; then
		# $gtmroutines is not set in the environment, attempt to pick it up from libgtmutil.so, $gtm_dist, $gtm_dist/plugin
		if [ -e "$gtm_dist/libgtmutil.so" ] ; then
			export gtmroutines="$gtm_dist/libgtmutil.so"
		elif [ -e "$gtm_dist/PINENTRY.o" ] ; then
			export gtmroutines="$gtm_dist"
		fi
	fi

	# Validate gtmroutines. Redirect output or it will affect the password protocol
	PINENTRY=PINENTRY
	printf 'zhalt (0=$zlength($text(pinentry^PINENTRY)))' | $gtm_dist/mumps -direct >> /dev/null 2>&1
	needsprivroutines=$?

	if [ 0 -ne "${needsprivroutines}" ] ; then
		PINENTRY=pinentry
		# Need to create a temporary directory for object routines
		if [ -x "`which mktemp 2>/dev/null`" ] ; then
			tmpdir=`mktemp -d`
		else
			tmpdir=/tmp/`basename $0`_$$.tmp ; mkdir $tmpdir
		fi
		trapstr="rm -rf $tmpdir"
		trap "$trapstr" HUP INT QUIT TERM TRAP
		export gtmroutines="$tmpdir($dir)"
	fi

	gtm_trace_gbl_name= gtmdbglvl= gtmcompile= $gtm_dist/mumps -run $PINENTRY
	punt=$?
	if [ -d "$tmpdir" ] ; then rm -rf "$tmpdir" ; fi
fi

if [ 0 -ne $punt ] ;then
	# Punt to the regular pinentry program
	pinentry=`which pinentry 2>/dev/null`
	if [ -x "$pinentry" ] ; then $pinentry $* ; else exit 1 ; fi
fi
