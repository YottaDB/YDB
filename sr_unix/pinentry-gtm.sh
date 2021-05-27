#!/bin/sh
#################################################################
#                                                               #
# Copyright (c) 2010-2017 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2018-2021 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
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
#	If the ydb_passwd/gtm_passwd environment variable exists and a usable mumps exists, run
#	pinentry.m to get the passphrase from the environment variable.
#
#############################################################################################

dir=`dirname $0` ; if [ -z "$dir" ] ; then dir=$PWD ; fi
punt=1

if [ -z "$ydb_dist" ] ; then
	# $ydb_dist is not set in the environment. See if we can use dirname to find one
	if [ "`echo $ydb_chset | tr utf UTF`" = "UTF-8" ] && [ -x "$dir/../../utf8/mumps" ] ; then
		ydb_dist=$dir/../../utf8
		export ydb_dist
	elif [ "`echo $gtm_chset | tr utf UTF`" = "UTF-8" ] && [ -x "$dir/../../utf8/mumps" ] ; then
		ydb_dist=$dir/../../utf8
		export ydb_dist
	elif [ -x "$dir/../../mumps" ] ; then
		ydb_dist=$dir/../..
		ydb_chset=M
		export ydb_dist ydb_chset
	fi
fi

if { [ -n "$ydb_passwd" ] || [ -n "$gtm_passwd" ]; } && [ -x "$ydb_dist/mumps" ] ; then
	pinentry=pinentry
	# Password and MUMPS exists, perform some extended setup checks
	if [ -z "$ydb_routines" ] && [ -z "$gtmroutines" ] ; then
		utfodir=""
		if [ "`echo $ydb_chset | tr utf UTF`" = "UTF-8" ] && [ -x "$dir/../../utf8/mumps" ] ; then
			utfodir="/utf8"
		elif [ "`echo $gtm_chset | tr utf UTF`" = "UTF-8" ] && [ -x "$dir/../../utf8/mumps" ] ; then
			utfodir="/utf8"
		fi
		# $ydb_routines & $gtmroutines is not set in the environment, attempt to pick it up from libyottadbutil.so,
		# $ydb_dist, $ydb_dist/plugin/o
		if [ -e "$ydb_dist/libyottadbutil.so" ] ; then
			ydb_routines="$ydb_dist/libyottadbutil.so"
			export ydb_routines
		elif [ -e "$ydb_dist/PINENTRY.o" ] ; then
			pinentry=PINENTRY
			ydb_routines="$ydb_dist"
			export ydb_routines
		elif [ -e "$ydb_dist/plugin/o${utfodir}/pinentry.o" ] ; then
			ydb_routines="$ydb_dist $ydb_dist/plugin/o${utfodir}"
			export ydb_routines
		fi
	fi

	# Protect the pinentry program from other env vars (unsetenv both ydb* and corresponding gtm* names)
	# Note: export of an undefined variable has no effect. It does not set the variable.
	unset ydb_env_translate ydb_etrap ydb_local_collate ydb_sysid ydb_trace_gbl_name ydb_zdate_form ydb_zstep ydb_zyerror ydb_compile ydb_dbglvl LD_PRELOAD	#BYPASSOKLENGTH
	ydb_ztrap_form=code
	export ydb_env_translate ydb_etrap ydb_local_collate ydb_sysid ydb_trace_gbl_name ydb_zdate_form ydb_zstep ydb_ztrap_form ydb_zyerror ydb_compile ydb_dbglvl LD_PRELOAD		#BYPASSOKLENGTH
	unset gtm_env_translate gtm_etrap gtm_local_collate gtm_sysid gtm_trace_gbl_name gtm_zdate_form gtm_zstep gtm_zyerror gtmcompile gtmdbglvl LD_PRELOAD	#BYPASSOKLENGTH
	gtm_ztrap_form=code
	export gtm_env_translate gtm_etrap gtm_local_collate gtm_sysid gtm_trace_gbl_name gtm_zdate_form gtm_zstep gtm_ztrap_form gtm_zyerror gtmcompile gtmdbglvl LD_PRELOAD		#BYPASSOKLENGTH

	# Validate ydb_routines. Redirect output or it will affect the password protocol
	# shellcheck disable=SC2016
	printf "%s" 'zhalt (0=$zlength($text(pinentry^'$pinentry')))' | $ydb_dist/mumps -direct >> /dev/null 2>&1
	needsprivroutines=$?

	if [ 0 -ne "${needsprivroutines}" ] ; then
		pinentry=pinentry
		# Need to create a temporary directory for object routines
		if [ -x $(command -v mktemp) ] ; then
			tmpdir=`mktemp -d`
		else
			tmpdir=/tmp/`basename $0`_$$.tmp ; mkdir $tmpdir
		fi
		trap 'rm -rf $tmpdir' HUP INT QUIT TERM TRAP
		pinentry_in="$dir"
		if [ -e "$ydb_dist/plugin/r/pinentry.m" ] ; then pinentry_in="$pinentry_in $ydb_dist/plugin/r"; fi
		if [ -e "$ydb_dist/plugin/gtmcrypt/pinentry.m" ] ; then pinentry_in="$pinentry_in $ydb_dist/plugin/gtmcrypt"; fi
		ydb_routines="$tmpdir($pinentry_in)"
		export ydb_routines
	fi

	$ydb_dist/mumps -run $pinentry
	punt=$?
	if [ -d "$tmpdir" ] ; then rm -rf "$tmpdir" ; fi
fi

if [ 0 -ne $punt ] ;then
	# Punt to the regular pinentry program
	pinentry=$(command -v pinentry)
	if [ -x "$pinentry" ] ; then $pinentry "$@" ; else exit 1 ; fi
fi
