#!/bin/sh
#################################################################
#								#
#	Copyright 2009, 2012 Fidelity Information Services, Inc	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

if [ $# -lt 1 ]; then
	echo "Usage: $0 ENCRYPTION_LIB [ALGORITHM]"
	echo "ENCRYPTION_LIB is either gcrypt or openssl"
	echo "	gcrypt : Install encryption plugin built with libgcrypt (if one exists)"
	echo "	openssl: Install encryption plugin built with OpenSSL (if one exists)"
	echo "ALGORITHM is either AES256CFB (default) or BLOWFISHCFB"
	echo "  AES256CFB      : Install encryption plugin built with AES (CFB mode) with 256-bit encryption"
	echo "  BLOWFISHCFB : Install encryption plugin built with BLOWFISH (CFB mode) with 256-bit encryption"
	exit 1
fi

# Since we are installing in the GT.M distribution directory, we better have $gtm_dist set and the plugin and plugin/gtmcrypt
# subdirectories present for us to do a successful copy.
if [ "" = $gtm_dist ]; then
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

encryption_lib=$1
algorithm="AES256CFB"
if [ $# -eq 2 ]; then algorithm="$2" ; fi

platform_name=`uname -s`
cwd=`pwd`
gtm_dist_plugin="$gtm_dist/plugin"

ext=".so"
if [ "OS/390" = $platform_name ]; then ext=".dll" ; fi

base_libname="libgtmcrypt"
generic_libname=$base_libname$ext
specific_libname=${base_libname}_${encryption_lib}_${algorithm}${ext}

echo "Installing M programs and shared libraries..."
if [ ! -f $cwd/$specific_libname ]; then
	echo "Shared library $specific_libname not built. Please check errors from build.sh"
	exit 1
fi

if [ ! -f $cwd/maskpass ]; then
	echo "Helper executable maskpass not built. Please check errors from build.sh"
	exit 1
fi

\rm -f $gtm_dist_plugin/$generic_libname			# Remove existing artifacts
if [ $cwd != $gtm_dist_plugin/gtmcrypt ]; then
	# Current directory is NOT a part of $gtm_dist/plugin. Just copy the shared libraries to $gtm_dist/plugin
	\cp $base_libname*$ext $gtm_dist_plugin
	\cp maskpass $gtm_dist_plugin/gtmcrypt/maskpass
else
	# Current directory is $gtm_dist/plugin/gtmcrypt. Move the shared libraries to $gtm_dist/plugin to avoid duplicate copies
	\mv $base_libname*$ext $gtm_dist_plugin
fi

\ln -s ./$specific_libname $gtm_dist_plugin/$generic_libname

cat << EOF > $gtm_dist_plugin/gpgagent.tab
$gtm_dist_plugin/$generic_libname
unmaskpwd: xc_status_t gc_pk_mask_unmask_passwd_interlude(I:xc_string_t*,O:xc_string_t*[512],I:xc_int_t)
EOF

cat << EOF > $gtm_dist_plugin/gtmcrypt/gtmcrypt.tab
getpass:char* getpass^GETPASS(I:gtm_int_t)
EOF

echo "Installation Complete"
exit 0
