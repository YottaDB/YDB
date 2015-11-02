#!/bin/ksh
#################################################################
#                                                               #
#       Copyright 2009 Fidelity Information Services, Inc 	#
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################

#############################################################################################
#
#       decrypt_db_key.ksh - decrypts the database secret key.
#	The user provides the passphrase of the private key in stdin.
#
#       Arguments:
#               $1 - Path of the encrypted key file
#
#############################################################################################

if [[ $# != 1 ]]; then
	echo "Usage: $0 <encrypted key file path>"
	exit 1
fi

encrypted_key_file=$1
if [[ ! -f $encrypted_key_file ]]; then
	echo "Cannot find $encrypted_key_file"
	exit 1
fi

print -n "Enter Password: "

stty -echo 2>/dev/null
read passphrase
stty echo 2>/dev/null

if [[ "" = $passphrase ]]; then
	echo "Passphrase cannot be empty"
	exit 1
fi

dir_path=`dirname $0`
echo $passphrase | gpg --no-tty --passphrase-fd 0 -d $encrypted_key_file | $dir_path/ascii2hex
echo
