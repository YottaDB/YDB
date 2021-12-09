#!/bin/bash

echo "Patching..."
patch ../sr_unix/sig_init.c sig_init.c.patch
mkdir build-instrumented
echo "Cmaking..."
cd build-instrumented
CC=$(which afl-gcc-fast) CXX=$(which afl-g++-fast) cmake ../../
echo "Making..."
make -j $(nproc)
