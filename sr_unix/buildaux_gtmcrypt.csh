#!/usr/local/bin/tcsh -f
#################################################################
#								#
# Copyright (c) 2001-2017 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
#
# Note: This script only works when called from buildaux.csh
#
echo ""
echo "############# Linking GTMCRYPT ###########"
echo ""
@ buildaux_gtmcrypt_status = 0
source $gtm_tools/gtm_env.csh

set supported_list = `$gtm_tools/check_encrypt_support.sh mail`
if ("ERROR" == "$supported_list") then
	# This is an error condition. Run check_encrypt_support in debug mode to have some debugging information.
	echo "buildaux-E-libgtmcrypt, $gtm_tools/check_encrypt_support.sh returned ERROR. Running it in debug mode before exiting"
	/bin/sh -x $gtm_tools/check_encrypt_support.sh
	exit 1
else if ("FALSE" == "$supported_list") then
	# This platform does not support encryption.
	echo "buildaux-I-libgtmcrypt, encryption is not supported on this platform"
	exit
endif
# Remove all lingering gpg-agent processes because they may have cached passphrases.
set gpg_agent_pids = `ps -ef | awk '/gpg-agent --homedir \/tmp\/gnupgdir\/'$USER' .*--daemon/ {print $2}'`
foreach gpg_agent_pid ($gpg_agent_pids)
	kill $gpg_agent_pid >&! /dev/null
end
set plugin_build_type=""
switch ($2)
	case "[bB]*":
		set plugin_build_type="PRO"
		breaksw
	case "[pP]*":
		set plugin_build_type="PRO"
		breaksw
	default:
		set plugin_build_type="DEBUG"
		breaksw
endsw
# First copy all the necessary source and script files to $gtm_dist/plugin/gtmcrypt
set helpers = "encrypt_sign_db_key,gen_keypair,gen_sym_hash,gen_sym_key,import_and_sign_key"
set helpers = "$helpers,pinentry-gtm,show_install_config"

set srcfiles = "gtmcrypt_dbk_ref.c gtmcrypt_pk_ref.c gtmcrypt_sym_ref.c gtmcrypt_ref.c gtm_tls_impl.c maskpass.c"
set srcfiles = "$srcfiles gtmcrypt_util.c"

set incfiles = "gtmcrypt_interface.h gtmcrypt_dbk_ref.h gtmcrypt_sym_ref.h gtmcrypt_pk_ref.h gtmcrypt_ref.h"
set incfiles = "$incfiles gtmcrypt_util.h gtm_tls_impl.h gtm_tls_interface.h"

set gtm_dist_plugin = $gtm_dist/plugin
rm -rf $gtm_dist_plugin
mkdir -p $gtm_dist_plugin/gtmcrypt
set srcfile_list = ($srcfiles)
eval cp -pf '${srcfile_list:gs||'$gtm_src'/|} $gtm_dist_plugin/gtmcrypt'

set incfile_list = ($incfiles)
eval cp -pf '${incfile_list:gs||'$gtm_inc'/|} $gtm_dist_plugin/gtmcrypt'

cp -pf $gtm_tools/{$helpers}.sh $gtm_dist_plugin/gtmcrypt
cp -pf $gtm_pct/pinentry.m $gtm_dist_plugin/gtmcrypt
rm -f $gtm_dist/{PINENTRY,pinentry}.[om]
cp -pf $gtm_tools/Makefile.mk $gtm_dist_plugin/gtmcrypt/Makefile
chmod +x $gtm_dist_plugin/gtmcrypt/*.sh
#
pushd $gtm_dist_plugin/gtmcrypt
if ("HP-UX" == "$HOSTOS") then
	set make = "gmake"
else
	set make = "make"
endif
if ($gtm_verno =~ V[4-8]*) then
	# For production builds don't do any randomizations.
	set algorithm = "AES256CFB"
	if ($HOSTOS == "AIX") then
		set encryption_lib = "openssl"
	else
		set encryption_lib = "gcrypt"
	endif
else
	# Randomly choose one configuration based on third-party library and algorithm.
	set rand = `echo $#supported_list | awk '{srand() ; print 1+int(rand()*$1)}'`
	set encryption_lib = $supported_list[$rand]
	if ("gcrypt" == "$encryption_lib") then
		# Force AES as long as the plugin is linked against libgcrypt
		set algorithm = "AES256CFB"
	else
		# OpenSSL, V9* build. AES256CFB is the only one we we officially support.
		set algorithm = "AES256CFB"
	endif
endif

source $gtm_tools/set_library_path.csh
source $gtm_tools/check_unicode_support.csh
if (! -e $gtm_dist/utf8) mkdir $gtm_dist/utf8
# Build and install all encryption libraries and executables.
env LC_ALL=$utflocale $make install algo=$algorithm image=$plugin_build_type thirdparty=$encryption_lib
if ($status) then
	@ buildaux_gtmcrypt_status++
	echo "buildaux-E-libgtmcrypt, failed to install libgtmcrypt and/or helper scripts"	\
				>> $gtm_log/error.${gtm_exe:t}.log
endif

# Remove temporary files.
$make clean
if ($status) then
	@ buildaux_gtmcrypt_status++
	echo "buildaux-E-libgtmcrypt, failed to clean libgtmcrypt and/or helper scripts"	\
				>> $gtm_log/error.${gtm_exe:t}.log
endif
# Remove pinentry routine for GTM-8668
rm -f $gtm_dist_plugin/gtmcrypt/pinentry.m


popd >&! /dev/null
exit $buildaux_gtmcrypt_status
