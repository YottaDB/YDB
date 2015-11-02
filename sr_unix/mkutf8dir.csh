#################################################################
#							                                	#
#	Copyright 2007 Fidelity Information Services, Inc     #
#								                                #
#	This source code contains the intellectual property	        #
#	of its copyright holder(s), and is made available	        #
#	under a license.  If you do not know the terms of	        #
#	the license, please stop and do not read further.	        #
#								                                #
#################################################################

#
###########################################################################################
#
#	mkutf8dir.csh - If ICU is installed, create utf8 subdirectory.
#                   Mirror parent directory except for .o's.
#	                Build .o's in UTF-8 mode.
#
###########################################################################################

if (`uname` == "AIX") then
	setenv LIBPATH /usr/local/lib
	set library_path = "$LIBPATH"
else
	setenv LD_LIBRARY_PATH "/usr/local/lib:/usr/lib"
	set library_path = `echo $LD_LIBRARY_PATH | sed 's/:/ /g'`
endif

set utflocale = `locale -a | grep -i en_us | grep -i utf | grep '8$'`
if (! -e utf8) then
	foreach libpath ($library_path)
		set found_icu = `ls -1 $libpath | grep "libicu.*36.*" | wc -l`
		if ($found_icu) then
			break
		endif
	end
	if ($found_icu && $utflocale != "") then
		mkdir utf8
	endif
endif

if (-e utf8) then
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
		set buildaux_status = `expr $buildaux_status + 1`
		echo "buildaux-E-compile_UTF8, Failed to compile .m programs in UTF-8 mode"
	endif
	cd ..
	setenv LC_CTYPE C
	unsetenv gtm_chset	# switch back to "M" mode
endif


