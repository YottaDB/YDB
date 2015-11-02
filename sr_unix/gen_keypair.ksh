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
#       gen_keypair.ksh - Generates a new public/private key pair for the current user.
#	The user's email address is provided along with the passphrase required to secure
#	the private key.
#
#       Arguments:
#               $1 -    Email ID of the current user.
#
#############################################################################################
gpghome=""
username="$USER"
pubkey="pubkey.asc"
HOSTOS=`uname -s`
ECHO=/bin/echo
ECHO_OPTIONS=""

#Linux honors escape sequence only when run with -e
if [[ "Linux" = $HOSTOS ]]; then
	ECHO_OPTIONS="-e"
fi

#email address is a mandatory parameter for GPG and hence can't be null.
if [[ $# -ne 1 ]]; then
	$ECHO "Usage: gen_keypair <email_address> "
	exit 1
fi
email=$1

dir_path=`dirname $0`

print -n "Enter Password: "

stty -echo 2>/dev/null
read passphrase
stty echo 2>/dev/null

echo ""
if [[ $passphrase = "" ]]; then
	$ECHO "Passphrase cannot be empty."
	exit 1
fi

#Fill out the unattended key generation details including the passphrase and email address
key_info="Key-Type: DSA\n Key-Length: 1024\n Subkey-Type: RSA\n Subkey-Length: 2048\n Name-Real: $username\n"
key_info=$key_info" Name-Email: $email\n Expire-Date: 0\n Passphrase: $passphrase\n %commit\n %echo Generated\n"

#If GNUPGHOME is already defined, then use this + .gnupg as the place to store the keys. If undefined,
#use $HOME/.gnupg (default for gpg)
if [[ $GNUPGHOME = "" ]]; then
        gpghome="$HOME/.gnupg"
else
        gpghome="$GNUPGHOME"
fi

$ECHO "Key generation might take a few minutes..."
#Spawn off gen_entropy.ksh in the background so that gpg has enough entropy when generating the key pair.
#Also, the user need not be troubled for generating entropy. We need to grab the pid of the gen_entropy
#so that we can kill it once gpg has successfully generated the key pairs.
$dir_path/gen_entropy.ksh &
entropy_pid=$!
#Start off a time out(10 minutes) script to kill the may-be-lingering entropy.
$dir_path/kill_entropy.ksh $entropy_pid &
kill_entropy_pid=$!

#Now, run the unattended gpg key generation. Any errors will be output'ed to gen_key.log
#which will later be removed.

$ECHO "GNUPGHOME=`echo $gpghome`"
tmp_file=/tmp/gen_key`date +%H_%M_%S`
$ECHO $ECHO_OPTIONS $key_info |gpg --homedir $gpghome --no-tty --batch --gen-key 2> $tmp_file

kill -9 $entropy_pid
kill -9 $kill_entropy_pid

gpg --homedir $gpghome --list-keys | grep "$email" > $tmp_file
if [[ $? = 0 ]]; then
	gpg --homedir $gpghome --export --armor -o $gpghome/../$pubkey
else
	$ECHO "Error creating public key/private key pairs."
	cat $tmp_file
fi

\rm -rf $tmp_file
