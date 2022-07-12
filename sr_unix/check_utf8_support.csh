#!/usr/local/bin/tcsh -f
#################################################################
#								#
# Copyright (c) 2007-2018 Fidelity National Information		#
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
set icu_ver = `ldconfig -p | grep -m1 -F libicuio.so. | cut -d" " -f1 | cut -d. -f3-4`
if ($icu_ver >= "36") then
	set found_icu = 1
else
	set found_icu = 0
endif

set utflocale = `locale -a | grep $binaryopt -iE '\.utf.?8$' | head -n1`

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
