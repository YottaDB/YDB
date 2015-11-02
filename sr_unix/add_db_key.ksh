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
#       add_db_key.ksh - Adds a new entry into the db key file. All path's provided in the
#	command line are converted to absolute paths and stored in the db key file.
#
#       Arguments:
#               $1 -    relative/absolute path of the database file
#               $2 -    relative/absolute path of the encrypted key file for this database($1)
#               $3 -    (optional) if provided, denotes the output file (db key file). If not
#			supplied, the value is taken from $gtm_dbkeys (if found empty/unset,
#			throws error)
#
#############################################################################################

#constructs the absolute path from the relative/absolute path
full_path()
{
	if echo $1 | grep "^[^/.]">>/dev/null; then
		fullpath="$(pwd)/$1"
	elif echo $1 | grep "^[.]">>/dev/null; then
		fullpath="$(pwd)/$1"
	else
		fullpath=$1
	fi
	return 0
}

if [[ $# -lt 2 ]]; then
	echo "Usage: add_db_key <database_file> <secret_key_file> [ tablefile ]"
	echo "Associates secret_key_file with database_file and stores the association in tablefile or $gtm_dbkeys (if exists)"
	return 1
fi

tablefile=$gtm_dbkeys
# check if third argument exists. if so, update the table filename
if [[ $# = 3 ]]; then
	full_path $3
	tablefile=$fullpath
elif [ "" = "$gtm_dbkeys" -a ! -e $HOME/.gtm_dbkeys ]; then
	tablefile=$HOME/.gtm_dbkeys
fi

full_path $1
dbfile=$fullpath
full_path $2
dbxfile=$fullpath

if [[ -d $tablefile ]]; then
	if [[ -f $tablefile/.gtm_dbkeys ]]; then
		tablefile=$tablefile/.gtm_dbkeys
	else
		echo "Cannot stat .gtm_dbkeys in $tablefile"
		exit 1
	fi
elif [[ "" = $tablefile ]]; then
	if [[ -f $HOME/.gtm_dbkeys ]]; then
		tablefile=$HOME/.gtm_dbkeys
	else
		echo "Environment variable - gtm_dbkeys undefined. Cannot stat $HOME/.gtm_dbkeys"
		exit 1
	fi
fi

echo "dat $dbfile" >> $tablefile
echo "key $dbxfile" >> $tablefile

exit 0
