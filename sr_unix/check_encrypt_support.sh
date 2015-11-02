#!/bin/sh
#################################################################
#								#
#	Copyright 2009 Fidelity Information Services, Inc #
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
if [ "OSF1" = "$hostos" -o \( "HP-UX" = "$hostos" -a "ia64" != `uname -m` \) ]; then
	echo "FALSE"
	exit 0
fi


file $gtm_dist/mumps | grep "64" > /dev/null
if [ $? -eq 0 ]; then
	lib_search_path="/usr/lib /usr/lib64 /usr/local/lib64 /lib64 /usr/local/lib"
else
	lib_search_path="/usr/lib /usr/local/lib"
fi
include_search_path="/usr/include /usr/local/include"
bin_search_path="/usr/bin /usr/local/bin /bin"

sym_headers="gcrypt.h"
sym_libs="libgcrypt.so"
if [ "AIX" = "$hostos" ]; then
	sym_headers="openssl/evp.h openssl/sha.h openssl/blowfish.h"
	sym_libs="libcrypto.a"
fi
req_inc_files="gpgme.h gpg-error.h $sym_headers"
req_lib_files="libgpgme.so libgpg-error.so $sym_libs"
req_bin_files="gpg ksh"

found_headers="TRUE"
found_libs="TRUE"
found_bins="TRUE"

for each_lib in $req_lib_files
do
	find_flag=0
	for each_lib_path in $lib_search_path
	do
		if [ -f $each_lib_path/$each_lib ]; then
			find_flag=1
			break
		fi
	done
	if [ $find_flag -eq 0 ]; then
		found_libs="FALSE"
		echo "FALSE"
		exit 0
		break
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
		echo "FALSE"
		exit 0
		break
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
		echo "FALSE"
		exit 0
		break
	fi
done

if [ $found_headers = "TRUE" -a $found_libs = "TRUE" -a $found_bins = "TRUE" ];then
	echo "TRUE"
	exit 0
else
	echo "FALSE"
	exit 0
fi
