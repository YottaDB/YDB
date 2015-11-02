#################################################################
#								#
#	Copyright 2007, 2010 Fidelity Information Services, Inc #
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
set utflocale = `locale -a | grep -i en_us | grep -i utf | grep '8$'`
set host_platform_name = `uname`
if ($host_platform_name == "AIX" || $host_platform_name == "SunOS" || $host_platform_name == "OS/390") then
	setenv LIBPATH /usr/local/lib64:/usr/local/lib
	setenv LD_LIBRARY_PATH /usr/local/lib64:/usr/local/lib
	set library_path = `echo $LIBPATH | sed 's/:/ /g'`
	if ("OS/390" == $HOSTOS) then
#		z/OS has both en_US.UTF-8 and En_US.UTF-8 with both .xplink and .lp64 suffixes - we need .lp64
		set utflocale = `locale -a | grep En_US.UTF-8.lp64 | sed 's/.lp64$//'`
	endif
else
# Optional parameter added. This script can be executed from remote system using ssh
# The environment $gtm_* variable's will not defined.
# Test system can pass an optional paramter of include path($gtm_inc) to check_unicode_support.csh
# to verify the presence of x86_64.h or s390.h (zLinux)
#
# Its worth noting that SuSE+RedHat,Debian & Ubuntu handle the lib32 vs lib64 differently
# Debian way: 		/lib		32bit		points to /emul/ia32-linux/lib
#			/lib64		64bit
# Ubuntu way:		/lib		ARCH default	points to either lib32 or lib64
#			/lib32		32bit
#			/lib64		64bit
# Redhat/SuSE way:	/lib		32bit
#			/lib64		64bit
	if ( $# == 1 ) then
		set incdir = "$1"
	else
		if ( $?gtm_inc ) then
			set incdir = "$gtm_inc"
		else
			set incdir = ""
		endif
	endif

	if (( -e $incdir/s390.h ) || ( -e $incdir/x86_64.h )) then
		setenv LD_LIBRARY_PATH "/usr/local/lib64:/usr/local/lib:/usr/lib64:/usr/lib"
	else
		setenv LD_LIBRARY_PATH "/usr/local/lib:/usr/lib:/usr/lib32"
        endif

	set library_path = `echo $LD_LIBRARY_PATH | sed 's/:/ /g'`
endif

set icu_ext = ".so"
if ($host_platform_name == "AIX") then
	set icu_ext = ".a"
else if ($host_platform_name == "HP-UX") then
	set icu_ext = ".sl"
endif
foreach libpath ($library_path)
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
	if (-e $libpath) then
		if (-f $libpath/libicuio$icu_ext) then
			set icu_versioned_lib = `ls -l $libpath/libicuio$icu_ext | awk '{print $NF}'`
			if ($host_platform_name == "AIX" || $host_platform_name == "OS/390") then
				set icu_ver = `echo $icu_versioned_lib | sed 's/libicuio//g' | cut -f 1 -d '.'`
			else
				set icu_ver = `echo $icu_versioned_lib | cut -f 3 -d '.'`
			endif
			if ($icu_ver >= "36") then
				set found_icu = 1
			else
				set found_icu = 0
			endif
		endif
	endif
	if ($found_icu) then
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
