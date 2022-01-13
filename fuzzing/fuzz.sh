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

set -e	# exit on error

PROCS=$(nproc)
INST=$((PROCS - 2))

mkdir env
cd env

# By default, afl-fuzz requires "performance" CPU frequency scaling whereas systems are most likely to have
# "on-demand" CPU frequency scaling. To change that, one would need root access. The consequence of not running
# with "performance" mode is some performance loss as mentioned below. So just set that env var before invoking afl-fuzz.
# From https://afl-1.readthedocs.io/en/latest/user_guide.html.
#	Setting AFL_SKIP_CPUFREQ skips the check for CPU scaling policy. This is useful if you can¿t change the
#	defaults (e.g., no root access to the system) and are OK with some performance loss.
export AFL_SKIP_CPUFREQ=1

# If EDITOR environment variable is set to a gui editor (e.g. gvim), ZEDIT commands that get tested by afl-fuzz below
# would invoke lots of gvim windows in the background and clutter the desktop. Prevent that by unsetting the env var
# in case it is set. In this case, a default editor would be chosen (usually vi) which does not open a new window.
unset EDITOR

# Fuzzing YottaDB will cause it to create files for sure (e.g. OPEN command will create a file).
# Since the leader and/or followers are going to each run their own "yottadb -direct" processes, it is
# possible multiple yottadb processes are manipulating the same file unintentionally at the same time.
# For example if one yottadb process is creating a file while another is deleting the same file etc.
# I am not sure exactly how but SUSPECT that this multi-process interference on the same file can explain
# various rare crashes that we saw during fuzz testing which we were not able to later reproduce.
# To avoid these external interferences, start each yottadb process in its own subdirectory.
echo "Creating leader..."
mkdir Leader
# Running in sub-shell below to avoid "cd .." at end (prevents SC2103 warning from shellcheck)
(
cd Leader
tmux new-session -d -s Leader "afl-fuzz -D -t 5000+ -i ../../inputs -o ../../output -M Leader -- ../../build-instrumented/yottadb -dir"
tmux new-session -d -s Cleaner "../../cleaner.sh"
)

echo "Creating followers..."
for i in $(seq $INST); do
	mkdir Follower$i
	# Running in sub-shell below to avoid "cd .." at end (prevents SC2103 warning from shellcheck)
	(
	cd Follower$i
	tmux new-session -d -s Follower"$i" "afl-fuzz -D -t 5000+ -i ../../inputs -o ../../output -S Follower$i -- ../../build-instrumented/yottadb -dir"
	tmux new-session -d -s Cleaner$i "../../cleaner.sh"
	)
done

echo "Fuzzers are running..."
