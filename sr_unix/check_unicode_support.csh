#################################################################
#								#
#	Copyright 2007, 2013 Fidelity Information Services, Inc #
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
	# 36 is the least version GT.M supports for ICU.
	# We have to get the numeric value from the ICU library. On non-AIX platforms, this can be done by
	# first getting the library to which libicuio.so is pointing to (this is always TRUE, in the sense
	# ICU always ships libicuio.so linked to the appropriate version'ed library).
	# The ICU libraries are formatted like "libicu<ALPHANUM>.<EXT>.<MAJOR_VER><MINOR_VER>". So, we can
	# use awk to get the last part of the 'ls -l' on libicuio.so and use awk and cut to get the version
	# numbers. However on AIX and z/OS, ICU libraries are formatted like "libicu<ALPHANUM><MAJOR_VER><MINOR_VER>.<EXT>"
	# and hence it is not as straightforward to extract the MAJOR_VER. So, we first eliminate the prefix
	# part of the library name that contains libicu<ALPHANUM> using sed and use cut now to extract the
	# version number.
	if ( ! -l $libdir ) continue

	set icu_versioned_lib = `ls -l $libdir | awk '{print $NF}'`

	if ($HOSTOS == "AIX" || $HOSTOS == "OS/390") then
		set icu_ver = `echo $icu_versioned_lib | sed 's/libicuio//g' | cut -f 1 -d '.'`
	else
		set icu_ver = `echo $icu_versioned_lib | cut -f 3 -d '.'`
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
