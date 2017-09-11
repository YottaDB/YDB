#!/bin/sh
#################################################################
#                                                               #
# Copyright (c) 2010-2017 Fidelity National Information		#
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
		gtm_dist=$dir/../../utf8
		export gtm_dist
	elif [ -x "$dir/../../mumps" ] ; then
		gtm_dist=$dir/../..
		gtm_chset=M
		export gtm_dist gtm_chset
	fi
fi

if [ -n "$gtm_passwd" -a -x "$gtm_dist/mumps" ] ; then
	pinentry=pinentry
	# Password and MUMPS exists, perform some extended setup checks
	if [ -z "$gtmroutines" ] ; then
		utfodir=""
		if [ "`echo $gtm_chset | tr utf UTF`" = "UTF-8" -a -x "$dir/../../utf8/mumps" ] ; then
			utfodir="/utf8"
		fi
		# $gtmroutines is not set in the environment, attempt to pick it up from libgtmutil.so,
		# $gtm_dist, $gtm_dist/plugin/o
		if [ -e "$gtm_dist/libgtmutil.so" ] ; then
			gtmroutines="$gtm_dist/libgtmutil.so"
			export gtmroutines
		elif [ -e "$gtm_dist/PINENTRY.o" ] ; then
			pinentry=PINENTRY
			gtmroutines="$gtm_dist"
			export gtmroutines
		elif [ -e "$gtm_dist/plugin/o${utfodir}/pinentry.o" ] ; then
			gtmroutines="$gtm_dist $gtm_dist/plugin/o${utfodir}"
			export gtmroutines
		fi
	fi

	# Protect the pinentry program from other env vars
	gtm_env_translate= gtm_etrap= gtm_local_collate= gtm_sysid= gtm_trace_gbl_name= gtm_zdate_form= gtm_zstep= gtm_ztrap_form=code gtm_zyerror= gtmcompile= gtmdbglvl= LD_PRELOAD=	#BYPASSOKLENGTH
	export gtm_env_translate gtm_etrap gtm_local_collate gtm_sysid gtm_trace_gbl_name gtm_zdate_form gtm_zstep gtm_ztrap_form gtm_zyerror gtmcompile gtmdbglvl LD_PRELOAD		#BYPASSOKLENGTH

	# Validate gtmroutines. Redirect output or it will affect the password protocol
	printf 'zhalt (0=$zlength($text(pinentry^'$pinentry')))' | $gtm_dist/mumps -direct >> /dev/null 2>&1
	needsprivroutines=$?

	if [ 0 -ne "${needsprivroutines}" ] ; then
		pinentry=pinentry
		# Need to create a temporary directory for object routines
		if [ -x "`which mktemp 2>/dev/null`" ] ; then
			tmpdir=`mktemp -d`
		else
			tmpdir=/tmp/`basename $0`_$$.tmp ; mkdir $tmpdir
		fi
		trapstr="rm -rf $tmpdir"
		trap "$trapstr" HUP INT QUIT TERM TRAP
		pinentry_in="$dir"
		if [ -e "$gtm_dist/plugin/r/pinentry.m" ] ; then pinentry_in="$pinentry_in $gtm_dist/plugin/r"; fi
		if [ -e "$gtm_dist/plugin/gtmcrypt/pinentry.m" ] ; then pinentry_in="$pinentry_in $gtm_dist/plugin/gtmcrypt"; fi
		gtmroutines="$tmpdir($pinentry_in)"
		export gtmroutines
	fi

	$gtm_dist/mumps -run $pinentry
	punt=$?
	if [ -d "$tmpdir" ] ; then rm -rf "$tmpdir" ; fi
fi

if [ 0 -ne $punt ] ;then
	# Punt to the regular pinentry program
	pinentry=`which pinentry 2>/dev/null`
	if [ -x "$pinentry" ] ; then $pinentry $* ; else exit 1 ; fi
fi
