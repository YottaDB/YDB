#! tcsh -f
#################################################################
#								#
#	Copyright 2007, 2008 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
#######################################################################
#
#	mkutf8dir.csh - Used by makefile build procedure
#		Should be kept in sync with similar code in comlist.csh
#			If ICU is installed, create utf8 subdirectory.
#                   Mirror parent directory except for .o's.
#	                Build .o's in UTF-8 mode.
#
#######################################################################

set checkunicode = "../sr_unix/check_unicode_support.csh"
if ( -e $checkunicode ) then
    set x8664inc = ""
    if ( "Linux" == `uname` && "x86_64" == `uname -m` && "64" == "$linux_build_type" ) set x8664inc = "../sr_x86_64"
    source $checkunicode $x8664inc
    if ("TRUE" == "$is_unicode_support") then
	if (! -e utf8) mkdir utf8
	setenv LC_CTYPE $utflocale
	unsetenv LC_ALL
	setenv gtm_chset UTF-8	# switch to "UTF-8" mode

	foreach file (*)
		# Skip directories
		if (-d $file) then
			continue
		endif
		# Skip soft linking .o files
		set extension = $file:e
		if ($extension == "o") then
			continue
		endif
		# Soft link everything else
		if (-e utf8/$file) then
			rm -f utf8/$file
		endif
		ln -s ../$file utf8/$file
	end

	cd utf8
	../mumps *.m
	if ($status != 0) then
		echo "mkutf8dir-E-compile_UTF8, Failed to compile .m programs in UTF-8 mode"
	endif
	cd ..
	setenv LC_CTYPE C
	unsetenv gtm_chset	# switch back to "M" mode
    endif
endif
