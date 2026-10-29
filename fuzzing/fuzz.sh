#!/usr/bin/env bash

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

# See comment in "fuzzing/instrument.sh" for why this is needed.
export AFL_IGNORE_PROBLEMS=1

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
# Each afl-fuzz instance runs one of the builds that "instrument.sh" created. Since "ydb_dist" has to point to the
# directory of the yottadb executable that is running (or else we get a YDBDISTUNVERIF error), set "ydb_dist" (and
# "ydb_routines" if it is set, as "enhanced_fuzz.csh" points it to build-instrumented) for each instance. These env vars
# are passed through "env" in the tmux command as a tmux session gets its environment from the tmux server, which is
# started by the first "tmux new-session" below.
# $1 : tmux session name (also the afl-fuzz instance name)
# $2 : -M (Leader) or -S (Follower)
# $3 : build directory name
start_fuzzer() {
	name=$1
	role=$2
	build=$3
	dist=$(cd "../../$build" && pwd)
	envvars="ydb_dist=$(printf %q "$dist")"
	if [ -n "${ydb_routines+set}" ]; then
		routines=${ydb_routines//\/build-instrumented\//\/$build\/}
		routines=${routines//\/build-instrumented)/\/$build)}
		envvars="$envvars ydb_routines=$(printf %q "$routines")"
	fi
	if [ "build-instrumented-asan" = "$build" ]; then
		# afl-fuzz requires abort_on_error=1 and symbolize=0. The rest are from YDBTest/com/set_asan_options_env_var.csh.
		envvars="$envvars ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:symbolize=0:unmap_shadow_on_exit=1:verify_asan_link_order=0"
	fi
	tmux new-session -d -s "$name" "env $envvars afl-fuzz -D -t 5000+ -i ../../inputs -o ../../output $role $name -- ../../$build/yottadb -dir"
	tmux new-session -d -s "Cleaner$name" "../../cleaner.sh"
}

echo "Creating leader..."
mkdir Leader
# Running in sub-shell below to avoid "cd .." at end (prevents SC2103 warning from shellcheck)
(
cd Leader
start_fuzzer Leader -M build-instrumented
)

# Follower1 runs the ASAN build. The remaining followers alternate between the Debug and RelWithDebInfo builds.
echo "Creating followers..."
for i in $(seq $INST); do
	if [ 1 -eq "$i" ]; then
		build="build-instrumented-asan"
	elif [ 0 -eq $((i % 2)) ]; then
		build="build-instrumented-dbg"
	else
		build="build-instrumented"
	fi
	mkdir Follower"$i"
	# Running in sub-shell below to avoid "cd .." at end (prevents SC2103 warning from shellcheck)
	(
	cd Follower"$i"
	start_fuzzer Follower"$i" -S $build
	)
done

echo "Fuzzers are running..."
