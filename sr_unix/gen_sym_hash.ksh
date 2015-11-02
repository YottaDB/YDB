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

################################################################################################
#
#       gen_sym_hash.ksh - generates SHA512 hash output of the given key + Encryption parameters
#	$1 - encrypted symmetric key
#
################################################################################################

if [ $# != 1 ]; then
	echo "Usage: $0 <encrypted symmetric key file>"
	exit 1
fi
encrypted_key_file="$1"
dir_path=`dirname $0`

hostos=`uname -s`

# Note that gtmcrypt_ref.h and gen_sym_hash.ksh NEED to use the same value for the encryption parameter string(defined below).
# This is currently determined by the OS type. If this changes, please verify both modules are changed. The variables in
# gtmcrypt_ref.h is UNIQ_ENC_PARAM_STRING and encr_param_string in this module.

if [ "AIX" = "$hostos" ]; then
	encr_param_string="BLOWFISHCFB"
else
	encr_param_string="AES256CFB"
fi

print -n "Enter Password: "
stty -echo 2> /dev/null
read passphrase
stty echo 2> /dev/null

echo ""

tmp_file="hash_tmp_$$_"`date +%H%M%S`
# We need to now decrypt the key, append the encryption parameter string (encr_param_string) above
# and compute SHA512 on the resulting string. Since we cannot store the symmetric key on the file,
# we first store the encryption parameter string (which is ok to be written to a file) and then
# decrypt the encrypted key file and cat the two in stdout. The result of this would be the
# symmetric key (plain form) appended with the encryption parameter string which will be sent to
# gpg for SHA512 computation. The result of the SHA512 has to be formatted in a single line and hence
# the usages of sed and tr.
print -n $encr_param_string > $tmp_file
cmd="gpg --no-tty -q --passphrase-fd 0 -d $encrypted_key_file"
echo $passphrase | $cmd | cat - $tmp_file | echo `gpg --print-md SHA512 | tr '\n' ' ' ` | sed 's/ //g'
