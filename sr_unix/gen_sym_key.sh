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
#       gen_sym_key.sh - generates a 32 byte random key using gpg --gen-random.
#       The generated key is stored in the output file encrypted with the user's public key.
#
#	$1 - Strength of the random bytes (0 - least; 2 - greatest)
#	$2 - Output file name for generated key encrypted with user's public key
#	$3-  Rest of line is treated as a comment
#
#############################################################################################

SYM_KEY_LEN=32

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

if [ $# -lt 2 ] ; then $ECHO Usage: "`basename $0` key_strength[0-2] output_file" ; exit 1 ; fi

# Identify GnuPG - it is required
if [ -x "`$which gpg 2>&1`" ] ; then gpg=gpg
elif [ -x "`$which gpg2 2>&1`" ] ; then gpg=gpg2
else  $ECHO "Able to find neither gpg nor gpg2.  Exiting" ; exit 1 ; fi

# Confirm ability to create output file
output_dir=`dirname $2` ; if [ -z "$output_dir" ] ; then output_dir=$PWD ; fi
if [ ! -w $output_dir ] ; then $ECHO "$output_dir does not exist or is not writable" ; exit 1 ; fi
if [ -f $2 ] ; then
    if [ ! -w $2 ] ; then $ECHO "Unable to overwrite existing output file $2" ; exit 1 ; fi
fi
output_file=$2

random_strength=3
case $1 in
    [0-2]) random_strength=$1 ;;
esac

while [ $random_strength -lt 0 -o $random_strength -gt 2 ] ; do
    $ECHO $ECHO_OPTIONS "Please enter a preferred strength for the key (0/1/2/[?]):" \\c
    read random_strength
    case "$random_strength" in
	[0-2]) ;;
	*)  random_strength=3
	    $ECHO
	    $ECHO "Choose a key strength 0 (weakest - for testing) through 2 (strongest - for production)."
	    $ECHO "Since 2 may use up all available entropy on your system and/or take some time"
	    $ECHO "it is recommended that you choose 2 only on desktop systems where you can"
	    $ECHO "more easily generate entropy by moving the mouse and typing."
	    $ECHO ;;
    esac
done

# Get comment for key
shift 2
comment="$*" ; if [ -z "$comment" ] ; then comment="Key in $output_file created by $USER `date -u`" ; fi

dir_path=`dirname $0` ; if [ -z "$dir_path" ] ; then dir_path=$PWD ; fi

# Generate random key and save the output encrypted and signed
$gpg --gen-random $random_strength $SYM_KEY_LEN | $gpg --armor --encrypt --default-recipient-self --comment "$comment" --output $output_file
