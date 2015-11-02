#!/bin/sh
#################################################################
#                                                               #
#       Copyright 2010 Fidelity Information Services, Inc #
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################

#############################################################################################
#
#       encrypt_db_key.sh - encrypts the symmetric database key with the recipient's public key.
#	The file is signed by the key provider.
#
#       Arguments:
#               $1 -    Input file with symmetric encryption key protected wiht user
#               $2 -    Path of the output file.
#               $3 -    Email address of the public key's owner.
#		Rest of line is comment for the output file
#
#############################################################################################

hostos=`uname -s`
# try to get a predictable which
if [ "OS/390" = "$hostos" ] ; then which=whence ;
elif [ -x "/usr/bin/which" ] ; then which=/usr/bin/which
else which=which
fi

# echo and options
ECHO=/bin/echo
ECHO_OPTIONS=""
#Linux honors escape sequence only when run with -e
if [ "Linux" = "$hostos" ] ; then ECHO_OPTIONS="-e" ; fi

# Input file, output file and recipient e-mail id are mandatory
if [ $# -lt 3 ]; then
    $ECHO "Usage: `basename $0` input_key_file output_key_file recipient_id" ; exit 1
fi

# Identify GnuPG - it is required
if [ -x "`$which gpg 2>&1`" ] ; then gpg=gpg
elif [ -x "`$which gpg2 2>&1`" ] ; then gpg=gpg2
else  $ECHO "Able to find neither gpg nor gpg2.  Exiting" ; exit 1 ; fi

# Confirm existence of and ability to read input file
if [ ! -r "$1" ] ; then $ECHO $1 does not exist or is not readable ; exit 1 ; fi
input_file=$1

# Confirm ability to create output file
output_dir=`dirname $2` ; if [ -z "$output_dir" ] ; then output_dir=$PWD ; fi
if [ ! -w $output_dir ] ; then $ECHO $output_dir does not exist or is not writable ; exit 1 ; fi
if [ -f $2 ] ; then
    if [ ! -w $2 ] ; then $ECHO Unable to overwrite output file $2 ; exit 1 ; fi
fi
output_file=$2

recipient=$3

# Get comment for key, create default if none provided on command line
shift 3
comment="$*" ; if [ -z "$comment" ] ; then comment="$output_file created from $input_file for $recipient by $USER `date -u`" ; fi

# Get passphrase for GnuPG keyring
$ECHO $ECHO_OPTIONS Passphrase for keyring: \\c ; stty -echo ; read passphrase ; stty echo ; $ECHO ""

# Yes, providing the passphrase on the command line to the second gpg command is not ideal, but that
# but that is the best we can do with this reference implementation.  Otherwise it must prompt twice.
echo $passphrase | $gpg --batch --passphrase-fd 0 --quiet --decrypt $input_file | $gpg --encrypt --armor --sign --output $output_file --comment "$comment" --recipient $recipient --batch --passphrase "$passphrase"
