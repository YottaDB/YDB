#!/bin/sh
#################################################################
#								#
#	Copyright 2009 Fidelity Information Services, Inc 	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

if [ ! $?gtm_dist ]; then
	echo "Environment variable - gtm_dist not defined."
	exit 1
fi

if [ ! -d $gtm_dist/plugin ]; then
	echo "Unable to locate $gtm_dist/plugin. Cannot install encryption library"
	exit 1
fi

if [ ! -d $gtm_dist/plugin/gtmcrypt ]; then
	echo "Unable to locate $gtm_dist/plugin/gtmcrypt. Cannot install helper scripts/executables"
	exit 1
fi

platform_name=`uname -s`
from_path=`pwd`
gtm_dist_plugin="$gtm_dist/plugin"
if [ "OS/390" = $platform_name ]; then
	lib_name="libgtmcrypt.dll"
else
	lib_name="libgtmcrypt.so"
fi

echo "Installing M programs and shared libraries..."
if [ ! -f $from_path/$lib_name ]; then
	echo "Shared library $lib_name not built. Please check errors from build.sh"
	exit 1
fi

if [ ! -f $from_path/ascii2hex ]; then
	echo "Helper executable ascii2hex not built. Please check errors from build.sh"
	exit 1
fi

if [ ! -f $from_path/maskpass ]; then
	echo "Helper executable maskpass not built. Please check errors from build.sh"
	exit 1
fi

if [ ! -f $from_path/GETPASS.o ]; then
	echo "GETPASS not compiled. Please check errors from build.sh"
	exit 1
fi

#Move the shared libraries to $gtm_dist/plugin
cp $from_path/$lib_name $gtm_dist_plugin/$lib_name

#Move maskpass to $gtm_dist/plugin/gtmcrypt
if [ "$from_path" != "$gtm_dist_plugin/gtmcrypt" ]; then
	cp $from_path/maskpass $gtm_dist_plugin/gtmcrypt/maskpass
fi

#Move ascii2hex pto $gtm_dist/plugin/gtmcrypt
if [ "$from_path" != "$gtm_dist_plugin/gtmcrypt" ]; then
	cp $from_path/ascii2hex $gtm_dist_plugin/gtmcrypt/ascii2hex
fi

#Move the password prompting M program to $gtm_dist
cp $from_path/GETPASS.o $gtm_dist/GETPASS.o

#Move the unicode version of the M program to $gtm_dist/utf8
if [ -d $gtm_dist/utf8 ]; then
	if [ ! -f $from_path/utf8/GETPASS.o ]; then
		echo "GETPASS not compiled in UTF-8 mode. Please check errors from build.sh"
		exit 1
	fi
	cp $from_path/utf8/GETPASS.o $gtm_dist/utf8/GETPASS.o
fi
echo "Installed $lib_name, GETPASS M utility"
exit 0
