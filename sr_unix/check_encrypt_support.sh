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
###########################################################################################
#
#	check_encrypt_support.sh - Checks if encryption library and executables required for
#	building encryption plugin are available
#	Returns :
#		TRUE - if ksh, gpg, crypt headers and libraries are available
#		FALSE - if not all of them are available i.e the host does not support encryption
###########################################################################################

hostos=`uname -s`
# We know we don't support these platforms
if [ "OSF1" = "$hostos" -o \( "HP-UX" = "$hostos" -a "ia64" != `uname -m` \) ]; then
	echo "FALSE"
	exit 0
fi

lib_search_path="/usr/local/lib64 /usr/local/lib /usr/lib64 /usr/lib /lib64 /lib /usr/local/ssl/lib /usr/lib/x86_64-linux-gnu"
lib_search_path="$lib_search_path /usr/lib/i386-linux-gnu /lib/x86_64-linux-gnu /lib/i386-linux-gnu"
include_search_path="/usr/include /usr/local/include /usr/local/include/gpgme"
bin_search_path="/usr/bin /usr/local/bin /bin"

sym_headers="gcrypt.h gcrypt-module.h"
req_lib_files="libgpg-error libgpgme"
lib_ext="so"
if [ "AIX" = "$hostos" ]; then
	sym_headers="openssl/evp.h openssl/sha.h openssl/blowfish.h"
	req_lib_files="libcrypto $req_lib_files"
	lib_ext="$lib_ext a"
else
	req_lib_files="libgcrypt $req_lib_files"
fi
if [ "OS/390" = "$hosotos" ]; then
	lib_ext="dll"
fi
req_inc_files="gpgme.h gpg-error.h $sym_headers"
req_bin_files="gpg ksh"

found_headers="TRUE"
found_libs="TRUE"
found_bins="TRUE"

whatsmissing=""

for each_lib in $req_lib_files
do
	find_flag=0
	for each_lib_path in $lib_search_path
	do
		for each_ext in $lib_ext
		do
			if [ -f $each_lib_path/$each_lib.$each_ext ]; then
				find_flag=1
				break
			fi
		done
	done
	if [ $find_flag -eq 0 ]; then
		found_libs="FALSE"
		whatsmissing="$whatsmissing library $each_lib is missing \n"
	fi
done
for each_header in $req_inc_files
do
	find_flag=0
	for each_path in $include_search_path
	do
		if [ -f $each_path/$each_header ]; then
			find_flag=1
			break
		fi
	done
	if [ $find_flag -eq 0 ]; then
		found_headers="FALSE"
		whatsmissing="$whatsmissing include file $each_header is missing \n"
	fi
done

for each_bin in $req_bin_files
do
	find_flag=0
	for each_path in $bin_search_path
	do
		if [ -x $each_path/$each_bin ]; then
			find_flag=1
			break
		fi
	done
	if [ $find_flag -eq 0 ]; then
		found_bins="FALSE"
		whatsmissing="$whatsmissing executable $each_bin is missing \n"
	fi
done

server_list="$cms_tools/server_list"

if [ -f $server_list ];then
	hostname=`uname -n | awk -F. '{print $1}'`
	eval `/usr/local/bin/tcsh -efc "
		source $server_list;
		echo non_encrypt_machines=\'\\\$non_encrypt_machines\';
		echo encrypt_desktops=\'\\\$encrypt_desktops\';
		echo encrypt_dist_servers=\'\\\$encrypt_dist_servers\';
		echo encrypt_other_servers=\'\\\$encrypt_other_servers\';
	" || echo echo ERROR \; exit 1`
	if [ $found_headers = "TRUE" -a $found_libs = "TRUE" -a $found_bins = "TRUE" ];then
	#	We want to keep at least one server without support for encryption.  So if it send a warning if it is not the case.
		echo $non_encrypt_machines | grep -w $hostname > /dev/null
		if [ $? -eq 0 ]; then
			errmsg="This system should not supports encryption, but it does.\n"
			printf "$errmsg" | mailx -s \
				"ENCRYPTSUPPORTED-W-WARNING : Server $hostname will build encryption plugin (but should not)" \
				gglogs
		fi
		echo "TRUE"
	else
	# 	These are distribution servers that support encryption. If can't build encryption plugin send error.
		echo $encrypt_dist_servers | grep -w $hostname > /dev/null
		if [ $? -eq 0 ]; then
			errmsg="Please setup the required dependencies for this distribution server:\n"
			errmsg="$errmsg$whatsmissing"
			printf "$errmsg" | mailx -s \
				"ENCRYPTSUPPORTED-E-ERROR : Distribution server $hostname will not build encryption plugin" \
				gglogs
		fi
	#	There are machines that can support encryption. If can't build encryption plugin send warning.
		encrypt_machines="$encrypt_other_servers $encrypt_desktops"
		echo $encrypt_machines | grep -w $hostname > /dev/null
		if [ $? -eq 0 ]; then
			errmsg="This system supports encryption but does not have the required dependencies setup:\n"
			errmsg="$errmsg$whatsmissing"
			printf "$errmsg" | mailx -s \
				"ENCRYPTSUPPORTED-W-WARNING : Server $hostname will not build encryption plugin" \
				gglogs
		fi
		echo "FALSE"
	fi
else
	if [ $found_headers = "TRUE" -a $found_libs = "TRUE" -a $found_bins = "TRUE" ];then
		echo "TRUE"
	else
		echo "FALSE"
	fi
fi

exit 0
