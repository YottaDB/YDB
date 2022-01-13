#!/bin/bash

#################################################################
#								#
# Copyright (c) 2021-2022 YottaDB LLC and/or its subsidiaries.	#
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

while true
do
	echo "Cleaning..."
	# Remove all files under current directory whose modification time is older than 1 minute
	find . -maxdepth 1 -type f -mmin +1 -delete
	sleep 10
done
