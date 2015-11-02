#!/bin/sh
#################################################################
#                                                               #
#       Copyright 2010, 2011 Fidelity Information Services, Inc #
#                                                               #
#       This source code contains the intellectual property     #
#       of its copyright holder(s), and is made available       #
#       under a license.  If you do not know the terms of       #
#       the license, please stop and do not read further.       #
#                                                               #
#################################################################

#############################################################################################
#
#       add_db_key.sh - Adds a new entry into the db key file. All paths provided in the
#	command line are converted to absolute paths and stored in the db key file.
#
#       Arguments:
#               $1 -    relative/absolute path of the database file (does not need to exist)
#               $2 -    relative/absolute path of the encrypted key file for this database($1)
#			(must exist)
#               $3 -    (optional) if provided, denotes the output file (db key file).
#			If not supplied, the value is taken from $gtm_dbkeys.
#			If $gtm_dbkeys doesn't exist, looks for $HOME/.gtm_dbkeys, which is
#			created if it does not exist.
#
#############################################################################################

# echo and options
ECHO=/bin/echo
ECHO_OPTIONS=""
#Linux honors escape sequence only when run with -e
if [ "Linux" = "`uname -s`" ] ; then ECHO_OPTIONS="-e" ; fi

# Database filename and key file name are required; master can default
if [ $# -lt 2 ]; then	$ECHO "Usage: `basename $0` database_file key_file [master_key_file]" ; exit 1 ; fi

case $2 in
    /*) keyfile=$2 ;;
    *) keyfile=$PWD/$2 ;;
esac
if [ ! -r "$keyfile" ] ; then $ECHO "Key file $keyfile does not exist or is not readable" ; exit 1 ; fi

case $1 in
    /*) dbfile=$1 ;;
    *) dbfile=$PWD/$1 ;;
esac

if [ $# -ge 3 ] ; then master=$3
elif [ -n "$gtm_dbkeys" ] ; then master=$gtm_dbkeys
else master=$HOME/.gtm_dbkeys
fi

if [ -d $master -o -f $master ] ; then
    if [ -w $master ] ; then
	if [ -d $master ] ; then master=$master/.gtm_dbkeys ; fi
    else
	$ECHO "$master is not writable" ; exit 1
    fi
else #Master does not exist; confirm that directory is writable
    tmp=`dirname $master` ; if [ -z "$tmp" ] ; then tmp=$PWD ; fi
    if [ ! -w $tmp ] ; then $ECHO "Directory $tmp not writable for master key file $master" ; exit 1 ; fi
fi

$ECHO dat $dbfile >>$master
$ECHO key $keyfile >>$master
