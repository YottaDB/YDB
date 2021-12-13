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
cp --no-clobber YDBTest/*/inref/*.m ./NotMinCorpus

echo "Removing overly large tests..."
cd NotMinCorpus || exit
find . -type f -size +1k -delete
cd ..

echo "Removing tests directory..."
rm -rf YDBTest

echo "Making env..."
mkdir env

(
cd env || exit
echo "Running afl-cmin..."
afl-cmin -i ../NotMinCorpus/ -o ../inputs -- ../build-instrumented/yottadb -dir
)

echo "Cleanup"
rm -rf env
rm -rf NotMinCorpus

echo "Done making corpus..."
