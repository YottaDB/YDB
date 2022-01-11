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
cd env || exit

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

echo "Creating leader..."
tmux new-session -d -s Leader "afl-fuzz -D -t 5000+ -i ../inputs -o ../output -M Leader -- ../build-instrumented/yottadb -dir"

for i in $(seq $INST); do
	tmux new-session -d -s Follower"$i" "afl-fuzz -D -t 5000+ -i ../inputs -o ../output -S Follower$i -- ../build-instrumented/yottadb -dir"
done

tmux new-session -d -s Cleaner "../cleaner.sh"
echo "Fuzzers are running..."
