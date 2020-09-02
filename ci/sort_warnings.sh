#!/bin/bash
#################################################################
#								#
# Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

warnings="$1"

# These don't impact the warning itself
printf "Remove 'note:' lines... "
sed "/:\( \)\?note: /d" "$warnings" > no_note.txt
echo "OK."

# Extract base filename
echo -n "Extracting basename... "
grep "^/" no_note.txt | cut -d " " -f 1 | sed 's!.*/!!' | cut -d ":" -f 1 > filenames.txt
echo "OK."

# Extract base warning message
echo -n "Extracting warning message... "
grep "^/" no_note.txt | cut -d " " -f 2- > warnings.txt
echo "OK."

# Concatenate filenames with warning messages and sort
echo -n "Combining and sorting filenames with messages... "
paste -d ": " filenames.txt warnings.txt | sort > sorted_warnings.txt
echo "OK."
