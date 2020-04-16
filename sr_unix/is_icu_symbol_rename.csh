#!/usr/local/bin/tcsh -f
#################################################################
#								#
# Copyright (c) 2009-2020 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# This tool crudely checks if it is possible to start gtm in utf8 mode. If "undefined symbol: u_getVersion" is seen,
# which is possible if icu is built with symbol renaming, then print icu version using pkg-config or icu-config
# The version that started support for gtm_icu_version is : V53004

if !($?gtm_dist) then
	setenv gtm_dist $gtm_root/$gtm_curpro/dbg/
endif
# depending on the list of locales configured, locale -a might be considered a binary output. (on scylla currently)
# grep needs -a option to process the output as text to get the actual value instead of "Binary file (standard input) matches"
# but -a is not supported on the non-linux servers we have.
if ("Linux" == "$HOSTOS") then
	set binaryopt = "-a"
else
	set binaryopt = ""
endif
set utflocale = `locale -a | grep $binaryopt -iE 'en_us\.utf.?8$' | head -n 1`
if ( -f $gtm_tools/set_library_path.csh ) then
	source $gtm_tools/set_library_path.csh >& /dev/null
endif
setenv LC_CTYPE $utflocale ; unsetenv LC_ALL
setenv gtm_chset UTF-8
set gtmstat = `echo "halt" | $gtm_dist/mumps -direct |& grep "u_getVersion"`
echo $gtmstat |& grep -q "u_getVersion"
if !($status) then
	# icu-config is deprecated. So try "pkg-config icu-io" first, followed by "icu-config" and "pkg-config icu"
	# Set cmd to icu-config, in case none of the conditions below satisfy. It is better to fail with error than exit quietly
	set cmd = "icu-config --version"
	if ( (-X pkg-config) && ( { pkg-config --exists icu-io } ) ) then
		set cmd = "pkg-config --modversion icu-io"
	else if (-X icu-config) then
		set cmd = "icu-config --version"
	else if ( (-X pkg-config) && ( { pkg-config --exists icu } ) ) then
		set cmd = "pkg-config --modversion icu"
	endif
	# pkg-config/icu-config can report versions like 4.2.1 but we want just the 4.2 part, so choose only upto the 2nd field
	echo `$cmd | awk '{ver=+$0;if(ver>5){ver=ver/10}printf("%.1f\n",ver);exit}'`
endif
