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
#       gen_sym_key.ksh - generates a 32 byte random key using gpg --gen-random. After
#	generating the random bytes, the script outputs the 64 byte Hex version of it which
#	will later be feeded for encrypt_db_key.ksh. This 64 byte hex output is only for display
#	purposes and will not be used for the real key.
#
#	$1 - Strength of the random bytes (0 - least; 2 - greatest)
#
#############################################################################################

SYM_KEY_LEN=32

if [[ $# -ne 1 ]]; then
	random_strength=2
else
	if [ $1 -gt 2 -o $1 -lt 0 ]; then
		echo "Invalid random strength. Please provide a number between 0 and 2"
		exit 1
	fi
	random_strength=$1
fi

dir_path=`dirname $0`
#run the gpg with --gen-random option and use ascii2hex to send output to stdout
sym=`gpg --gen-random $random_strength $SYM_KEY_LEN | $dir_path/ascii2hex`
print -n $sym
