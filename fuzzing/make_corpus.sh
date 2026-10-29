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

if [ -d ./inputs ]; then
	echo "corpus already exists? (directory named 'inputs' already in pwd)"
	echo "exiting..."
	exit 0
fi

echo "Cloning test repository..."
git clone https://gitlab.com/YottaDB/DB/YDBTest.git

echo "Making unminified corpus directory..."
mkdir NotMinCorpus

echo "Copying tests..."
# Look for .m files under com and */inref
# It is possible M file names are the same across different directories.
# To avoid copying one from clobbering the other, prefix destination file name with the source directory.
# And use split to ensure each file is not more than 1024 bytes long (helps afl with smaller input).
for file in YDBTest/com/*.m
do
	base=${file##*/}
	basename=${base%.*}
	cp $file ./NotMinCorpus/com${basename}.m
done

for file in YDBTest/*/inref/*.m
do
	base=${file##*/}
	basename=${base%.*}
	dir=${file%/*}
	dir2=${dir%/*}
	testname=${dir2##*/}
	cp $file ./NotMinCorpus/${testname}${basename}.m
done

echo "Splitting overly large tests into smaller pieces..."
(
cd NotMinCorpus
for file in *.m
do
	basename=${file%.*}
	# --suffix-length=4 indicates use 4 digits to represent piece number at end (e.g. 0005 for 5th piece).
	# So the file ydb546.m would be split and created as ydb5460001.m ydb5460002.m, etc.
	split --suffix-length=4 --line-bytes=1024 --additional-suffix=.m --numeric-suffixes $file $basename
	rm $file	# remove original file (which is a duplicate) now that we have split it into multiple pieces
done
)

# If FUZZ_SEED_DIR is set, it is a directory of inputs saved from an earlier fuzzing campaign (for example the
# "output/Leader/queue" directory of that campaign). Add those inputs too so a new campaign (e.g. with a newer YottaDB
# build or with different random env vars from "enhanced_fuzz.csh") starts from what the earlier campaign found
# instead of from just the YDBTest tests. "afl-cmin" below drops the inputs that do not add coverage.
if [ -n "$FUZZ_SEED_DIR" ]; then
	echo "Copying inputs from $FUZZ_SEED_DIR..."
	num=0
	find "$FUZZ_SEED_DIR" -maxdepth 1 -type f | while read -r file
	do
		num=$((num + 1))
		cp "$file" ./NotMinCorpus/seed$num
	done
fi

echo "Removing tests directory..."
rm -rf YDBTest

echo "Making env..."
mkdir env

(
cd env || exit

# Set env var before invoking yottadb. See comment in "fuzzing/instrument.sh" for why this is needed.
export AFL_IGNORE_PROBLEMS=1

# The yottadb executable loads libyottadb.so using dlopen(). The coverage of a library that is loaded after the afl
# forkserver starts is not recorded, so preload libyottadb.so. Also set AFL_MAP_SIZE to the coverage map size that the
# executable and the preloaded library need. See "fuzz.sh" for more details.
export AFL_PRELOAD=$PWD/../build-instrumented/libyottadb.so
# afl prints the map size and exits with a non-zero status if AFL_DUMP_MAP_SIZE is set, hence the "|| true".
AFL_MAP_SIZE=$(AFL_DUMP_MAP_SIZE=1 LD_PRELOAD=$AFL_PRELOAD ../build-instrumented/yottadb -dir < /dev/null) || true
export AFL_MAP_SIZE

echo "Running afl-cmin..."
afl-cmin -i ../NotMinCorpus/ -o ../inputs -- ../build-instrumented/yottadb -dir
)

echo "Cleanup"
rm -rf env
rm -rf NotMinCorpus

echo "Done making corpus..."
