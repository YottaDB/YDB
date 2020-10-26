#!/bin/sh

#################################################################
#								#
# Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# Determines whether a file should need a copyright by its name
# Returns 0 if it needs a copyright and 1 otherwise.
# Returns 2 if an error occurs.
set -eu

if ! [ $# = 1 ]; then
	echo "usage: $0 <filename>"
	exit 2
fi

file="$1"

# Don't require deleted files to have a copyright
if ! [ -e "$file" ]; then
       exit 1
fi

# Below is a list of specific files that do not have a copyright so ignore them
skiplist="COPYING LICENSE README.md sr_port/gdeinitsz.m sr_port/md5hash.c sr_port/md5hash.h"
skiplist="$skiplist sr_unix/custom_errors_sample.txt sr_unix/gtmgblstat.xc"
skiplist="$skiplist sr_port/copyright.txt"
if echo "$skiplist" | grep -q -w "$file"; then
	exit 1
fi

# A few file extensions do not have copyright
skipextensions="hlp list exp md ref"
if echo "$skipextensions" | grep -q -w "$(echo "$file" | awk -F . '{print $NF}')"; then
	exit 1
fi

# A few specific filenames do not have copyright as they are generated
skipbasenames="GTMDefinedTypesInitRelease.m GTMDefinedTypesInitDebug.m"
if echo "$skipbasenames" | grep -q -w "$(basename "$file")"; then
	exit 1
fi
