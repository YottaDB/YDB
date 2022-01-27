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

echo "Removing tests directory..."
rm -rf YDBTest

echo "Making env..."
mkdir env

(
cd env || exit

# Set env var before invoking yottadb. See comment in "fuzzing/instrument.sh" for why this is needed.
export AFL_IGNORE_PROBLEMS=1

echo "Running afl-cmin..."
afl-cmin -i ../NotMinCorpus/ -o ../inputs -- ../build-instrumented/yottadb -dir
)

echo "Cleanup"
rm -rf env
rm -rf NotMinCorpus

echo "Done making corpus..."
