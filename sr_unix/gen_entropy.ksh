#!/bin/ksh
#################################################################
#								#
#	Copyright 2009 Fidelity Information Services, Inc #
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
#This script generates entropy by running a find / command and pushing it to the background and piping the 2>&1 to /dev/null
#This will be called from gen_keypair.ksh and once gpg found enough entropy to create key pair and terminates, the caller kills
#this script using kill -9. This script can be later modified if there is a need for a much cleaner entropy generation.

a=0;
while [ $a -eq 0 ]; do
	if [[ ! -w /dev/random ]]; then
		echo $RANDOM > /dev/null
	else
		echo $RANDOM > /dev/random
	fi
done
