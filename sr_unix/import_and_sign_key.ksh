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
#       import_and sign_key.ksh: Import public key into the owner's keyring. After confirming
#	the fingerprint, sign the key.
#
#	Arguments -
#	$1 - path of the public key file.
#	$2 - email id of the public key's owner.
#
#############################################################################################

if [ $# -ne 2 ]; then
	echo "Usage: $0 <public_key_file> <email_id>"
	exit 1
fi

public_key_file=$1
email_id=$2

#Run a grep on available public keys in the keyring and figure out
#if this email id is already present in the keyring. If so, we need
#not import again (although importing it once again might not really
#create problem)
gpg --list-keys | grep $email_id
if [ $? -ne 0 ]; then
	#Make sure the path of the public key is valid. If not we terminate
	if [ ! -a $public_key_file ]; then
		echo "$public_key_file not found."
		exit 1
	fi

	print -n "Are you sure you want to import $public_key_file ($email_id) ? (y/n):"
	read confirmation

	if [ "y" != $confirmation ]; then
		echo "Not importing the public key."
		exit 1
	fi
	#Try importing the public key into the keyring. If there were any errors
	#while importing, display an error message and terminate.
	gpg --no-tty --import --yes $public_key_file
	if [ $? -ne 0 ]; then
		echo "Error importing the public key - $public_key_file"
		exit 1
	fi
	#Display fingerprint of the just imported public key
	echo "#########################################################"
	gpg --fingerprint $email_id
	if [ $? -ne 0 ]; then
		echo "Error obtaining fingerprint the email id - $email_id"
		exit 1
	fi
	echo "#########################################################"
	#Confirm with the user if the fingerprint is valid.
	print -n "Please confirm the validity of the the fingerprint before signing (y/n):"
	read confirmation
	echo ""
	if [ "y" != $confirmation ]; then
		echo "Not signing the public key."
		exit 1
	fi
	#If yes, we need to sign the public key. In order to do so, we need the user's passphrase.
	print "Enter Password: "
	stty -echo 2>/dev/null
	read passphrase
	stty echo 2>/dev/null
	echo ""
	#We have all the pre-requisites for signing. So, go ahead and do the signing.
	echo $passphrase | gpg --no-tty --batch --passphrase-fd 0 --sign-key --yes $email_id
	if [ $? -eq 0 ]; then
		echo "Successfully signed $public_key_file ($email_id)."
		exit 0
	else
		echo "Error signing $public_key_file ($email_id)."
		exit 1
	fi
else
	echo "$email_id already imported."
	exit 1
fi
