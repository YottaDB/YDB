#################################################################
#								#
#	Copyright 2007 Fidelity Information Services, Inc 	#
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

set found_icu = ""
set utflocale = `locale -a | grep -i en_us | grep -i utf | grep '8$'`
if (`uname` == "AIX") then
	setenv LIBPATH /usr/local/lib
	set library_path = "$LIBPATH"
else
	setenv LD_LIBRARY_PATH "/usr/local/lib:/usr/lib:/usr/lib32"
	set library_path = `echo $LD_LIBRARY_PATH | sed 's/:/ /g'`
endif
foreach libpath ($library_path)
	# 36 is the version GT.M supports for ICU
	if (-e $libpath) set found_icu = `ls -1 $libpath | grep "libicu.*36.*" | wc -l`
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
