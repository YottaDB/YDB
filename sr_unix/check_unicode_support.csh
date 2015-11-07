#################################################################
#								#
#	Copyright 2007, 2014 Fidelity Information Services, Inc #
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
#	check_unicode_support.csh - Checks if icu library and utf8 locale is available
#	setenv is_unicode_support to TRUE/FALSE
#	Returns :
#		TRUE - if both icu library and utf8 locale is installed
#		FALSE - if either of them is not available
###########################################################################################

set found_icu = 0
set utflocale = `locale -a | grep -i en_us | grep -i utf | grep '8$' | head -n 1`
if ("OS/390" == $HOSTOS) then
#	z/OS has both en_US.UTF-8 and En_US.UTF-8 with both .xplink and .lp64 suffixes - we need .lp64
	set utflocale = `locale -a | grep En_US.UTF-8.lp64 | sed 's/.lp64$//' | head -n 1`
endif

# This _could_ not work on new platforms or newly installed supported platforms.
# It should be manually tested using this command :
#    ssh <some host> ls -l {/usr/local,/usr,}/lib{64,,32}/libicuio.{a,so,sl}

foreach libdir ( {/usr/local,/usr,}/lib{64,/x86_64-linux-gnu,,32,/i386-linux-gnu}/libicuio.{a,so,sl} )
	# 36 is the least version GT.M supports for ICU. We have to get the numeric value from the ICU library.
	# ICU ships libicuio.so linked to the appropriate versioned library - so using filetype -L works well
	# The below is the format of the libraries on various platforms:
	# AIX, z/OS : libicu<alphanum><majorver><minorver>.<ext>   (e.g libicuio42.1.a)
	# Others    : libicu<alphanum>.<ext>.<majorver>.<minorver> (e.g libicuio.so.42.1)

	if ( ! -l $libdir ) continue

	set icu_versioned_lib = `filetest -L $libdir`
	set verinfo = ${icu_versioned_lib:s/libicuio//}
	set parts = ( ${verinfo:as/./ /} )

	if ($HOSTOS == "AIX" || $HOSTOS == "OS/390") then
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
# The calling gtm installation script should sould source this script in order to avoid duplication of 'setenv LD_LIBRARY_PATH'
# The gtm-internal test system runs it within `...` in a few places and sets the return value to an env variable
# To aid both the cases above, do a 'setenv is_unicode_support' as well as 'echo' of TRUE/FALSE
if ($found_icu && $utflocale != "") then
	setenv is_unicode_support TRUE
	echo "TRUE" # the system has unicode/utf8 support
else
	setenv is_unicode_support FALSE
	echo "FALSE" # the system doesn't have unicode/utf8 support
endif
