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

echo "Patching..."
make patch

mkdir build-instrumented
echo "Cmaking..."
cd build-instrumented || exit

set +e	# temporarily disable "exit on error" as "which" commands below can error out saying command does not exist.
# Check if afl-clang-fast exists. If so use it. If not check afl-gcc-fast. If so use it. If neither exists, issue error.
ccompiler=$(which afl-clang-fast)
if [ "" = "$ccompiler" ] ; then
	ccompiler=$(which afl-gcc-fast)
fi
if [ "" = "$ccompiler" ] ; then
	echo "instrument.sh: Neither [afl-clang-fast] nor [afl-gcc-fast] exist. Exiting..."
	exit 1
fi

# Check if afl-clang-fast++ exists. If so use it. If not check afl-g++-fast. If so use it. If neither exists, issue error.
cppcompiler=$(which afl-clang-fast++)
if [ "" = "$cppcompiler" ] ; then
	cppcompiler=$(which afl-g++-fast)
fi
if [ "" = "$cppcompiler" ] ; then
	echo "instrument.sh: Neither [afl-clang-fast++] nor [afl-g++-fast] exist. Exiting..."
	exit 1
fi
set -e	# Re-enable "exit on error" now that "which" commands are done.

CC=$ccompiler CXX=$cppcompiler cmake ../../

# On Debian 11 and Ubuntu 21.10, gcc (10 and 11 versions respectively) did not work when building afl++ from source.
# Setting the CC env var to clang during that build process let that build work. When the resulting "afl-clang-fast" was used
# instrument YottaDB in the "make" below, we saw errors like the following towards the end of building YottaDB.
#	[-] FATAL: forkserver is already up, but an instrumented dlopen() library loaded afterwards.
#	You must AFL_PRELOAD such libraries to be able to fuzz them or LD_PRELOAD to run outside of afl-fuzz.
#	To ignore this set AFL_IGNORE_PROBLEMS=1.
# Setting this env var stopped the above message and fuzzing worked without issues. Hence the below line.
export AFL_IGNORE_PROBLEMS=1

echo "Making..."
make -j "$(nproc)"

# Undo source code changes now that build is done. This allows later "git status" commands to not show the patch as a diff.
echo "Undoing patch..."
cd ..
make reset

