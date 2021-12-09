#!/bin/bash

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
cd NotMinCorpus
find -type f -size +1k -delete
cd ..

echo "Removing tests directory..."
rm -rf YDBTest

echo "Making env..."
mkdir env
cd env

echo "Running afl-cmin..."
afl-cmin -i ../NotMinCorpus/ -o ../inputs -- ../build-instrumented/yottadb -dir

cd ..

echo "Cleanup"
rm -rf env
rm -rf NotMinCorpus

echo "Done making corpus..."
