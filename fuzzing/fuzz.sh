#!/usr/bin/env bash

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

PROCS=$(nproc)
INST=$((PROCS - 2))

mkdir env
cd env || exit

echo "Creating leader..."
tmux new-session -d -s Leader "afl-fuzz -D -t 5000+ -i ../inputs -o ../output -M Leader -- ../build-instrumented/yottadb -dir"

for i in $(seq $INST); do
	tmux new-session -d -s Follower"$i" "afl-fuzz -D -t 5000+ -i ../inputs -o ../output -S Follower$i -- ../build-instrumented/yottadb -dir"
done

tmux new-session -d -s Cleaner "../cleaner.sh"
echo "Fuzzers are running..."
