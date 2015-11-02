#!/bin/sh
#################################################################
#								#
#	Copyright 2009, 2010 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# Install encryption plugin artifacts (libgtmcrypt.so and maskpass) from the build directory to GT.M distribution
# directory

# Since we are installing in the GT.M distribution directory, we better have $gtm_dist set and the plugin and plugin/gtmcrypt
# subdirectories present for us to do a successful copy.
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

if [ ! -f $from_path/maskpass ]; then
	echo "Helper executable maskpass not built. Please check errors from build.sh"
	exit 1
fi

# Create external call table needed by pinenty-gtm
cat > $gtm_dist_plugin/gpgagent.tab << tabfile
$gtm_dist_plugin/$lib_name
unmaskpwd: xc_status_t gc_pk_mask_unmask_passwd_interlude(I:xc_string_t*,O:xc_string_t*[512],I:xc_int_t)
tabfile

# Move the shared libraries to $gtm_dist/plugin
\cp $from_path/$lib_name $gtm_dist_plugin/$lib_name

if [ "$from_path" = "$gtm_dist_plugin"/gtmcrypt ] ; then
	# The build is done from the distribution directory. No need to install"
	echo "Installation Complete"
	exit 0
fi

# Move maskpass to $gtm_dist/plugin/gtmcrypt
\cp $from_path/maskpass $gtm_dist_plugin/gtmcrypt/maskpass

echo "Installation Complete"
exit 0
