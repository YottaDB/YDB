#! tcsh -f
#################################################################
#								#
#	Copyright 2007, 2011 Fidelity Information Services, Inc	#
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
    set incdir = ""
    if ( "Linux" == `uname` && "64" == "$gt_build_type" ) then
	switch (`uname -m`)
	case "x86_64":
	    set incdir = "../sr_x86_64"
	    breaksw
	case "s390x":
	    set incdir = "../sr_s390"
	    breaksw
	default:
	    breaksw
	endsw
    endif
    source $checkunicode $incdir
    if ("TRUE" == "$is_unicode_support") then
	if (! -e utf8) mkdir utf8
	if ( "OS/390" == $HOSTOS ) then
		setenv gtm_chset_locale $utflocale	# LC_CTYPE not picked up right
	endif
	setenv LC_CTYPE $utflocale
	unsetenv LC_ALL
	setenv gtm_chset UTF-8	# switch to "UTF-8" mode

	foreach file (*)
		# Skip utf8 directory
		if (-d $file && "utf8" == $file) then
			continue
		endif
		# Skip soft linking .o files
		set extension = $file:e
		if ($extension == "o") then
			continue
		endif
		# Soft link everything else
		if (-e utf8/$file) then
			rm -rf utf8/$file
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
