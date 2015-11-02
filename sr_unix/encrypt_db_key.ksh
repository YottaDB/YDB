#!/bin/ksh
#################################################################
#                                                               #
#       Copyright 2009 Fidelity Information Services, Inc #
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################

#############################################################################################
#
#       encrypt_db_key.ksh - encrypts the database secret key with the user's public key.
#	The user's public key is provided along with the email id of the public key's owner.
#
#       Arguments:
#               $1 -    Hex representation of the symmetric key (output'ed by gen_sym_key.ksh.
#               $2 -    Path of the output file.
#               $3 -    Email address of the public key's owner.
#
#############################################################################################

SYM_KEY_LEN=64
HOSTOS=`uname -s`
ECHO=/bin/echo

if [[ $# -ne 3 ]]; then
	$ECHO "Usage: encrypt_db_key.ksh <hex_symmetric_key> <output_file> <email_id> "
	$ECHO "[Encrypts the symmetric key with the public key provided and stores it in the path mentioned by the output file]"
	return 1
fi

hex_sym_key=$1
output_file=$2
email_id=$3

#Make sure the key length is 64 bytes.
if [[ ${#hex_sym_key} -ne $SYM_KEY_LEN ]]; then
	$ECHO "Symmetric key length is not $SYM_KEY_LEN"
	$ECHO "$hex_sym_key"
	exit 1
fi

#Read two bytes at a time from the hexadecimal input and convert it
#to '0' prefixed octal output which can be easily spitted out by echo
#to the expect script that will take it and encrypt it in the output file
idx=1;
bin_sym_key=""
while [ $idx -le "${#hex_sym_key}" ]; do
	two_byte=$(print "$hex_sym_key" | cut -c "${idx}-$((idx+1))")
	dec_byte=`printf "%d" 0x$two_byte`
	bin_sym_key=$bin_sym_key"\\0`printf %o $dec_byte`";
	idx=`expr $idx + 2`
done
if [[ "AIX" = $HOSTOS || "SunOS" = $HOSTOS || "HP-UX" = $HOSTOS ]]; then
	$ECHO "$bin_sym_key\c" | gpg -a -e -o $output_file -r $email_id
else
	print -n $bin_sym_key | gpg -a -e -o $output_file -r $email_id
fi
if [[ $? -ne 0 ]]; then
	$ECHO "Error encrypting the symmetric key."
	exit 1
fi
