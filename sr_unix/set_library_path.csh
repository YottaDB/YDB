#################################################################
#								#
#	Copyright 2010, 2012 Fidelity Information Services, Inc #
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
#	set_library_path.csh - setenv LIBPATH and LD_LIBRARY_PATH
#
#       The calling gtm installation script should sould source this script in order to
#	avoid duplication of 'setenv LD_LIBRARY_PATH'.
###########################################################################################

if ($HOSTOS == "AIX" || $HOSTOS == "SunOS" || $HOSTOS == "OS/390") then
	setenv LIBPATH /usr/local/lib64:/usr/local/lib
	setenv LD_LIBRARY_PATH /usr/local/lib64:/usr/local/lib
else
# Its worth noting that SuSE+RedHat, Debian & Ubuntu handle the lib32 vs lib64 differently
# Debian way: 		/lib		32bit		points to /emul/ia32-linux/lib
#			/lib64		64bit
#	Debian 7 layout is uniform across archs, but is more annoying than before
# 		/usr/lib/i386-linux-gnu	  32bit
#		/usr/lib/x86_64-linux-gnu 64bit
# Ubuntu way:		/lib		ARCH default	points to either lib32 or lib64
#			/lib32		32bit
#			/lib64		64bit
# Redhat/SuSE way:	/lib		32bit
#			/lib64		64bit
	if ( ! $?gtm_inc ) then
		echo "ERROR : gtm_inc not defined!"
		exit
	endif

# please keep in sync with sr_unix/gtm_test_install.csh
	if (( -e $gtm_inc/s390.h ) || ( -e $gtm_inc/x86_64.h )) then
		setenv LD_LIBRARY_PATH "/usr/local/lib64:/usr/local/lib:/usr/lib64:/usr/lib/x86_64-linux-gnu:/usr/lib"
	else
		setenv LD_LIBRARY_PATH "/usr/local/lib:/usr/lib:/usr/lib/i386-linux-gnu:/usr/lib32"
        endif
endif
