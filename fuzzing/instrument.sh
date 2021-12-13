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

echo "Patching..."
patch ../sr_unix/sig_init.c sig_init.c.patch
mkdir build-instrumented
echo "Cmaking..."
cd build-instrumented || exit
CC=$(which afl-gcc-fast) CXX=$(which afl-g++-fast) cmake ../../
echo "Making..."
make -j "$(nproc)"
