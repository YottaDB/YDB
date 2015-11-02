#!/bin/sh
#################################################################
#                                                               #
#       Copyright 2010, 2012 Fidelity Information Services, Inc 	#
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
# try to get a predictable which
if [ "OS/390" = "$hostos" ] ; then which=whence ;
elif [ -x "/usr/bin/which" ] ; then which=/usr/bin/which
else which=which
fi

if [ ! -x $basedir/show_install_config.sh ]; then
	echo "Cannot find show_install_config.sh in $basedir. Exiting"
	exit 1
fi
algorithm=`$basedir/show_install_config.sh | awk '/^ALGORITHM/ {print $NF}'`
# temporary file
if [ -x "`$which mktemp 2>&1`" ] ; then tmp_file=`mktemp`
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
if [ -x "`$which gpg 2>&1`" ] ; then gpg=gpg
elif [ -x "`$which gpg2 2>&1`" ] ; then gpg=gpg2
else  $ECHO "Able to find neither gpg nor gpg2.  Exiting" ; exit 1 ; fi

# Get passphrase for GnuPG keyring
$ECHO $ECHO_OPTIONS Passphrase for keyring: \\c ; stty -echo ; read passphrase ; stty echo ; $ECHO ""

$ECHO $passphrase | $gpg --no-tty --batch --passphrase-fd 0 -d $encrypted_key_file | cat - $tmp_file | $gpg --print-md SHA512 | tr -d ' \n'
$ECHO

rm -f $tmp_file

