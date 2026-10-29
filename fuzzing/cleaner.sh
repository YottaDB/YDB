#!/bin/bash

#################################################################
#								#
# Copyright (c) 2021-2026 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
# Original code by Zachary Minneker from Security Innovation.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# It is possible the "find" command below errors out (due to concurrent changes to files in the directory)
# In that case, we don't want to exit the cleaner job. We want to ignore this error and move on with
# cleaning future files that get generated. Hence we comment the below "set -e" line out which might seem
# inconsistent with other *.sh files in the "fuzzing/" directory.
#
# set -e	# exit on error

# $1 is the afl-fuzz instance name (e.g. Leader, Follower1). Used to name the directory where core files are kept.
coredir=../cores/$1
maxcores=5
mkdir -p "$coredir"

# If "fuzz.sh" created a database for this instance (in the "db" subdirectory), the globals and triggers that fuzzed
# processes create stay in it and are seen by every later fuzzed process of this instance. Over time that grows the
# database and makes the behavior of an input depend on the inputs that ran before it (e.g. a trigger one input created
# fires in the updates of a later input). So remove all triggers and globals in it every 6 iterations of the loop below
# (about once a minute), and give back the space they used. An input can write hundreds of megabytes before afl-fuzz
# times it out, and a KILL frees the blocks inside the database file without making the file smaller.
dbgld=$PWD/db/yottadb.gld
dbdist=$(cd ../../build-instrumented && pwd)
iteration=0

while true
do
	echo "Cleaning..."
	iteration=$((iteration + 1))
	if [ -e "$dbgld" ] && [ 0 -eq $((iteration % 6)) ]; then
		(
		# Use M mode and the default routines, like "fuzz.sh" does to create the database
		unset ydb_routines gtmroutines ydb_chset gtm_chset
		export LC_ALL=C ydb_dist=$dbdist ydb_gbldir=$dbgld AFL_IGNORE_PROBLEMS=1
		# Delete the triggers first so a trigger that fuzzed code created does not run (or fail) during the KILLs
		cd db || exit
		# shellcheck disable=SC2016
		"$dbdist/yottadb" -run %XCMD 'set x=$ztrigger("item","-*")' < /dev/null > purge.log 2>&1
		# shellcheck disable=SC2016
		"$dbdist/yottadb" -run %XCMD 'set g="^%" for  set g=$order(@g) quit:g=""  kill @g' < /dev/null >> purge.log 2>&1
		"$dbdist/mupip" reorg -truncate -region DEFAULT < /dev/null >> purge.log 2>&1
		)
	fi
	# "fuzz.sh" sets AFL_ALLOW_CORES so a crash creates a core file in the current directory. afl-fuzz keeps the crashing
	# inputs in "output/<instance name>/crashes", so a core file is only needed for a crash that does not reproduce from
	# its input outside the fuzzing environment. Keep the first few core files of this instance for such crashes. Delete
	# the rest as an input that crashes often would otherwise fill the disk with core files. The kept core files are the
	# first ones of this instance whatever the crash, so a crash that comes after them has no core file kept.
	find . -maxdepth 1 -type f -name 'core*' | while read -r core
	do
		if [ "$(find "$coredir" -type f | wc -l)" -lt $maxcores ]; then
			mv "$core" "$coredir/${core#./}_$(date +%Y%m%d_%H%M%S)"
		else
			rm -f "$core"
		fi
	done
	# Remove all files under current directory whose modification time is older than 1 minute
	find . -maxdepth 1 -type f -mmin +1 -delete
	sleep 10
done
