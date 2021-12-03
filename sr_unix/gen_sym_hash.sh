#!/bin/sh
#################################################################
#                                                               #
# Copyright (c) 2010-2021 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################

################################################################################################
#
#       gen_sym_hash.sh - generates SHA512 hash output of the given key + Encryption parameters
#	$1 - encrypted symmetric key
#
################################################################################################

# echo and options
# Linux honors escape sequence only when run with -e
# gtmcrypt_ref.h and gen_sym_hash.sh NEED to use the same value for
# the encryption parameter string(defined below).
# This is currently determined by the OS type. If this changes,
# please verify that UNIQ_ENC_PARAM_STRING in gtmcrypt_ref.h
# and encr_param_string in this module match.

if [ $# -lt 1 ]; then
	$ECHO "Usage: $0 <encrypted symmetric key file>" ; exit 1
fi
hostos=`uname -s`
basedir=`dirname $0`

if [ ! -x $basedir/show_install_config.sh ]; then
	echo "Cannot find show_install_config.sh in $basedir. Exiting"
	exit 1
fi
algorithm=`$basedir/show_install_config.sh | awk '/^ALGORITHM/ {print $NF}'`
# temporary file
if [ -x "$(command -v mktemp)" ] ; then tmp_file=`mktemp`
else tmp_file=/tmp/`basename $0`_$$.tmp ; fi
touch $tmp_file
chmod go-rwx $tmp_file
trap 'rm -rf $tmp_file ; stty sane ; exit 1' HUP INT QUIT TERM TRAP

ECHO=/bin/echo
ECHO_OPTIONS=""
if [ "Linux" = $hostos ] ; then ECHO_OPTIONS="-e" ; fi;

encrypted_key_file="$1"

$ECHO $ECHO_OPTIONS $algorithm\\c >$tmp_file

# Identify GnuPG - it is required
gpg=`command -v gpg2`
if [ -z "$gpg" ] ; then gpg=`command -v gpg` ; fi
if [ -z "$gpg" ] ; then $ECHO "Unable to find gpg2 or gpg. Exiting" ; exit 1 ; fi

# Get passphrase for GnuPG keyring
$ECHO $ECHO_OPTIONS Passphrase for keyring: \\c ; stty -echo ; read passphrase ; stty echo ; $ECHO ""

$ECHO $passphrase | $gpg --no-tty --batch --passphrase-fd 0 -d $encrypted_key_file | \
	cat - $tmp_file | $gpg --print-md SHA512 | tr -d ' \n'
$ECHO

rm -f $tmp_file
