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
#       import_and sign_key.sh: Import public key into the owner's keyring. After confirming
#	the fingerprint, sign the key.
#
#	Arguments -
#	$1 - path of the public key file.
#	$2 - email id of the public key's owner.
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

# Path to key file and email id are required
if [ $# -lt 2 ]; then
    $ECHO  "Usage: `basename $0` public_key_file email_id"
    exit 1
fi
public_key_file=$1
email_id=$2

# Identify GnuPG - it is required
if [ -x "`$which gpg 2>&1`" ] ; then gpg=gpg
elif [ -x "`$which gpg2 2>&1`" ] ; then gpg=gpg2
else  $ECHO "Able to find neither gpg nor gpg2.  Exiting" ; exit 1 ; fi

# Exit if the public key for this id already exists in the keyring
$gpg --list-keys $email_id 2>/dev/null 1>/dev/null
if [ $? -eq 0 ] ; then
    $ECHO  "Public key of $email_id already exists in keyring." ; exit 1
fi

# Ensure that the public key file exists and is readable
if [ ! -r $public_key_file ] ; then
    $ECHO  "Key file $public_key_file not accessible." ; exit 1
fi

# Import the public key into the keyring
$gpg --no-tty --import --yes $public_key_file
if [ $? -ne 0 ] ; then
    $ECHO  "Error importing public key for $email_id from $public_key_file" ; exit 1
fi

# Display fingerprint of the just imported public key
$ECHO  "#########################################################"
$gpg --fingerprint $email_id
if [ $? -ne 0 ] ; then
    $ECHO  "Error obtaining fingerprint the email id - $email_id" ; exit 1
fi
$ECHO  "#########################################################"

trap 'stty sane ; exit 1' HUP INT QUIT TERM TRAP

# Confirm with the user whether the fingerprint matches
unset tmp
while [ "Y" != "$tmp" ] ; do
    $ECHO $ECHO_OPTIONS "Please confirm validity of the fingerprint above (y/n/[?]):" \\c
    read tmp ; tmp=`$ECHO $tmp | tr yesno YESNO`
    case $tmp in
	"Y"|"YE"|"YES") tmp="Y" ;;
	"N"|"NO") $ECHO Finger print of public key for $email_id in $public_key_file not confirmed
	    $gpg --no-tty --batch --delete-keys --yes $email_id
	    exit 1 ;;
	*) $ECHO
	    $ECHO "If the fingerprint shown above matches the fingerprint you have been indepently"
	    $ECHO "provided for the public key of this $email_id, then press Y otherwise press N"
	    $ECHO ;;
    esac
done
unset tmp


#If yes, we need to sign the public key. In order to do so, we need the user's passphrase.
# Get passphrase for GnuPG keyring
$ECHO $ECHO_OPTIONS Passphrase for keyring: \\c ; stty -echo ; read passphrase ; stty echo ; $ECHO ""

# Export and sign the key
$ECHO  $passphrase | $gpg --no-tty --batch --passphrase-fd 0 --sign-key --yes $email_id
if [ $? -eq 0 ]; then
    $ECHO  "Successfully signed public key for $email_id received in $public_key_file" ; exit 0
else
    $gpg --no-tty --batch --delete-keys --yes $email_id
    $ECHO  "Failure signing public key for $email_id received in $public_key_file" ; exit 1
fi
