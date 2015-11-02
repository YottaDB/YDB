#!/bin/sh
#################################################################
#								#
#	Copyright 2012 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

if [ "x" = x"$gtm_dist" ]; then
	echo "Environment variable - gtm_dist not defined."
	exit 1
fi

if [ ! -d $gtm_dist/plugin ]; then
	echo "Unable to locate $gtm_dist/plugin. Exiting..."
	exit 1
fi

platform_name=`uname -s`
ext=".so"
if [ "OS/390" = $platform_name ]; then ext=".dll" ; fi

base_libname="libgtmcrypt"
generic_libname=$base_libname$ext
if [ "x" = x"$gtm_crypt_plugin" ]; then
	shared_object="$gtm_dist/plugin/$generic_libname"
	txt="symbolic link pointed by $gtm_dist/plugin/$generic_libname"
else
	shared_object="$gtm_dist/plugin/$gtm_crypt_plugin"
	txt='$gtm_crypt_plugin'
fi
if [ ! -f $shared_object ] ; then
	echo "Cannot find $shared_object. Exiting..."
	exit 1
fi
# Obtain the symbolic link (if any)
link=`ls -l $shared_object | awk '{print $NF}'`
# Get rid of the prefix (any path associated with the link) and the extension
basepart=`echo $link | awk -F/ '{print $NF}' | sed 's/'"$ext"'$//'`
# Resulting $basepart should be of form -- A_B_C
encryption_lib=`echo $basepart | cut -f2 -d'_'`
algorithm=`echo $basepart | cut -f3 -d'_'`
if [ "$encryption_lib" = "$algorithm" -o "" = "$algorithm" ] ; then
	echo "Unable to determine encryption library name or algorithm. Please ensure that $txt has the correct format. Exiting..."
	exit 1
fi

echo "ENCRYPTION_LIB = $encryption_lib"
echo "ALGORITHM = $algorithm"
exit 0
