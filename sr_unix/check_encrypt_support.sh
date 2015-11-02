#!/bin/sh
#################################################################
#								#
#	Copyright 2009, 2012 Fidelity Information Services, Inc #
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#################################################################
#								#
#	check_encrypt_support.sh - Checks if headers, libraries	#
#				   and executables required for	#
#				   building encryption plugin	#
#				   are available		#
#	Returns - If encryption is supported, returns either	#
#		  "libgcrypt" or "openssl" or "libgcrypt openssl#
#		  FALSE, otherwise.				#
#################################################################

#################################
#	Helper functions	#
#################################
check_files()
{
	# Check if a given list of files with a given list of extensions is
	# present in a given list of search paths
	srch_path=$1
	srch_files=$2
	srch_ext=$3
	missing=""
	ret=0
	for each_file in $srch_files
	do
		flag=0
		for each_path in $srch_path
		do
			for each_ext in $srch_ext
			do
				if [ -f $each_path/$each_file$each_ext ]; then flag=1 break ; fi
			done
			# The below takes care of files whose extensions are already determined by the caller
			if [ -f $each_path/$each_file ]; then flag=1 break; fi
		done
		if [ $flag -eq 0 ]; then
			missing="$missing $each_file"
			ret=1
		fi
	done
	return $ret
}

send_mail()
{
	msg=$1
	echo $encrypt_dist_servers | grep -w $hostname > /dev/null
	if [ $? -eq 0 ]; then
		msg="Please setup the required dependencies for this distribution server.\n\n$msg"
		msg="$msg\nAt least one of Libgcrypt or OpenSSL dependencies must be met."
		sub="ENCRYPTSUPPORTED-E-ERROR : Distribution server $hostname will not build encryption plugin"
	fi
	echo "$encrypt_other_servers $encrypt_desktops" | grep -w $hostname > /dev/null
	if [ $? -eq 0 ]; then
		msg="This system supports encryption but does not have the required dependencies setup\n\n.$msg"
		msg="$msg\nAt least one of Libgcrypt or OpenSSL dependencies must be met."
		sub="ENCRYPTSUPPORTED-W-WARNING : Server $hostname will not build encryption plugin"
	fi
	printf "$msg" | mailx -s "$sub" gglogs
}

#################################
#	Helper functions ends	#
#################################

hostos=`uname -s`
hostname=`uname -n | awk -F. '{print $1}'`
machtype=`uname -m`
server_list="$cms_tools/server_list"
this_host_noencrypt="FALSE"
if [ -f $server_list ]; then
	eval `/usr/local/bin/tcsh -efc "
		source $server_list;
		echo non_encrypt_machines=\'\\\$non_encrypt_machines\';
		echo encrypt_desktops=\'\\\$encrypt_desktops\';
		echo encrypt_dist_servers=\'\\\$encrypt_dist_servers\';
		echo encrypt_other_servers=\'\\\$encrypt_other_servers\';
		" || echo echo ERROR \; exit 1`
	echo $non_encrypt_machines | grep -w $hostname > /dev/null
	if [ $? -eq 0 ]; then
		this_host_noencrypt="TRUE"
	fi
fi

if [ "OSF1" = "$hostos" -o \( "HP-UX" = "$hostos" -a "ia64" != "$machtype" \) -o "TRUE" = "$this_host_noencrypt" ]; then
	echo "FALSE"
	exit 0
fi

lib_search_path="/usr/local/lib64 /usr/local/lib /usr/lib64 /usr/lib /lib64 /lib /usr/local/ssl/lib /usr/lib/x86_64-linux-gnu"
lib_search_path="$lib_search_path /usr/lib/i386-linux-gnu /lib/x86_64-linux-gnu /lib/i386-linux-gnu /opt/openssl/0.9.8/lib/hpux64"
include_search_path="/usr/include /usr/local/include /usr/local/include/gpgme /usr/local/ssl/include /opt/openssl/0.9.8/include"
bin_search_path="/usr/bin /usr/local/bin /bin"

mandate_headers="gpgme.h gpg-error.h"
mandate_libs="libgpg-error libgpgme"
mandate_bins="gpg"
gcrypt_headers="gcrypt.h gcrypt-module.h"
gcrypt_libs="libgcrypt"
openssl_headers="openssl/evp.h openssl/sha.h openssl/blowfish.h"
openssl_libs="libcrypto"
lib_ext=".so"
if [ "AIX" = "$hostos" ]; then
	lib_ext="$lib_ext .a"
elif [ "OS/390" = "$hosotos" ]; then
	lib_ext=".dll"
fi

# ------------------------------------------------------ #
# 		Mandatory checks			 #
# ------------------------------------------------------ #
found_mandates="TRUE"
check_files "$include_search_path" "$mandate_headers" ""
if [ $? -eq 1 ]; then
	found_mandates="FALSE"
	mandate_missing_list="Mandatory header files - $missing - are missing"
fi

check_files "$lib_search_path" "$mandate_libs" "$lib_ext"
if [ $? -eq 1 ]; then
	found_mandates="FALSE"
	mandate_missing_list="$mandate_missing_list\nMandatory library files - $missing - are missing"
fi

check_files "$bin_search_path" "$mandate_bins" ""
if [ $? -eq 1 ]; then
	found_mandates="FALSE"
	mandate_missing_list="$mandate_missing_list\nMandatory executables - $missing - are missing"
fi


# ------------------------------------------------------ #
# 		Check for libgcrypt			 #
# ------------------------------------------------------ #
found_gcrypt="TRUE"
check_files "$include_search_path" "$gcrypt_headers" ""
if [ $? -eq 1 ]; then
	found_gcrypt="FALSE"
	gcrypt_missing_list="\nLibgcrypt header files - $missing - are missing"
fi

check_files "$lib_search_path" "$gcrypt_libs" "$lib_ext"
if [ $? -eq 1 ]; then
	found_gcrypt="FALSE"
	gcrypt_missing_list="$gcrypt_missing_list\nLibgcrypt library files - $missing - are missing"
fi

# ------------------------------------------------------ #
# 		Check for OpenSSL			 #
# ------------------------------------------------------ #
found_openssl="TRUE"
check_files "$include_search_path" "$openssl_headers" ""
if [ $? -eq 1 ]; then
	found_openssl="FALSE"
	openssl_missing_list="\nOpenSSL header files - $missing - are missing"
fi

check_files "$lib_search_path" "$openssl_libs" "$lib_ext"
if [ $? -eq 1 ]; then
	found_openssl="FALSE"
	openssl_missing_list="$openssl_missing_list\nOpenSSL library files - $missing - are missing\n"
fi

# ------------------------------------------------------ #
# 		Figure out supported list 		 #
# ------------------------------------------------------ #
mail_msg=""
supported_list=""
if [ "TRUE" = $found_mandates ] ; then
	if [ "TRUE" = "$found_gcrypt" ]; then supported_list="$supported_list gcrypt" ; fi
	if [ "TRUE" = "$found_openssl" ]; then supported_list="$supported_list openssl" ; fi
	if [ "" != "$supported_list" ] ; then
		echo $supported_list
		exit 0
	fi
fi
# Some of the dependencies are unmet.
mail_msg="$mandate_missing_list \n \n $gcrypt_missing_list \n \n $openssl_missing_list"
if [ "mail" = "$1" ]; then
	if [ -f $server_list ]; then
		send_mail "$mail_msg"
	fi
fi
echo "FALSE"
exit 1
