#!/usr/local/bin/tcsh -f
#################################################################
#								#
# Copyright (c) 2007-2023 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
# Copyright (c) 2017-2022 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Copyright (c) 2018 Stephen L Johnson. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
#
###########################################################################################
#
#	check_utf8_support.csh - Checks if icu library and utf8 locale is available
#	setenv is_utf8_support to TRUE/FALSE
#	Returns :
#		TRUE - if both icu library and utf8 locale is installed
#		FALSE - if either of them is not available
###########################################################################################

# depending on the list of locales configured, locale -a might be considered a binary output.
# grep needs -a option to process the output as text but -a is not supported on the non-linux servers we have.
if ("Linux" == "$HOSTOS") then
	set binaryopt = "-a"
else
	set binaryopt = ""
endif

# The below logic finds the current ICU version. This is copied over from similar code in sr_unix/ydbinstall.sh
# The sed is needed at the end to remove any non-numeric characters in the version file (e.g. suse65 -> 65).
set icu_ver = `ldconfig -p | grep -m1 -F libicuio.so. | cut -d" " -f1 | cut -d. -f3 | sed 's/[^0-9]//g'`
if ($icu_ver >= "36") then
	set found_icu = 1
else
	set found_icu = 0
endif

set utflocale = `locale -a | grep $binaryopt -iE '\.utf.?8$' | head -n1`

<<<<<<< HEAD
=======
# icu-config is deprecated. So try "pkg-config icu-io" first, followed by "icu-config" and "pkg-config icu"
set cmd = "echo 0"
if ( -X lslpp ) then
	@ hasadt = { lslpp -Lcq ICU4C.adt } >& /dev/null
	if ($hasadt) then
		set uvernum="/usr/icu4c/include/unicode/uvernum.h"
		set cmd = 'awk '"'"'$2 == "U_ICU_VERSION_SHORT" { ver=$3 ; gsub(/"/,"",ver) ; print ver }'"' $uvernum"
		unset uvernum
	endif
	unset hasadt
else if ( (-X pkg-config) && ( { pkg-config --exists icu-io } ) ) then
	set cmd = "pkg-config --modversion icu-io"
else if (-X icu-config) then
	set cmd = "icu-config --version"
else if ( (-X pkg-config) && ( { pkg-config --exists icu } ) ) then
	set cmd = "pkg-config --modversion icu"
endif
set icuver="`$cmd`"
set found_icu = `awk -v ver="$icuver" 'BEGIN {if(ver>=3.6) {print 1} else {print 0}}'`
unset icuver

if (0 == $found_icu) then
	# If ICU is not found using the method above, just try harder by looking for libicuio*.* files in known locations
	# This could not work on new platforms or newly installed supported platforms.
	# It should be manually tested using this command :
	#    ssh <some host> ls -l {/usr/local,/usr,}/lib{64,,32}/libicuio.{a,so,sl}
	foreach libdir ( {/usr/local,/usr,}/lib{64,/x86_64-linux-gnu,,32,/i386-linux-gnu}/libicuio.{a,so,sl} )
		# 36 is the least version GT.M supports for ICU. We have to get the numeric value from the ICU library.
		# ICU ships libicuio.so linked to the appropriate versioned library - so using filetype -L works well
		# The below is the format of the libraries on various platforms:
		# AIX       : libicu<alphanum><majorver><minorver>.<ext>   (e.g libicuio42.1.a)
		# Others    : libicu<alphanum>.<ext>.<majorver>.<minorver> (e.g libicuio.so.42.1)

		if ( ! -l $libdir ) continue

		set icu_versioned_lib = `filetest -L $libdir`
		set verinfo = ${icu_versioned_lib:s/libicuio//}
		set parts = ( ${verinfo:as/./ /} )

		if ($HOSTOS == "AIX") then
			# for the above example parts = (42 1 a)
			set icu_ver = $parts[1]
		else
			# for the above example parts = (so 42 1)
			set icu_ver = $parts[2]
		endif

		if ($icu_ver >= "36") then
			set found_icu = 1
			break
		endif
	end
endif
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
# The calling gtm installation script should sould source this script in order to avoid duplication of 'setenv LD_LIBRARY_PATH'
# The gtm-internal test system runs it within `...` in a few places and sets the return value to an env variable
# To aid both the cases above, do a 'setenv is_utf8_support' as well as 'echo' of TRUE/FALSE
if ($found_icu && $utflocale != "") then
	setenv is_utf8_support TRUE
	echo "TRUE" # the system has utf8 support
else
	setenv is_utf8_support FALSE
	echo "FALSE" # the system doesn't have utf8 support
endif
