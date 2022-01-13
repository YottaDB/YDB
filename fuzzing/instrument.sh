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
patch ../sr_port/io.h io.h.patch
patch ../sr_port/jobexam_process.c jobexam_process.c.patch
patch ../sr_unix/ojchildioset.c ojchildioset.c.patch
patch ../sr_unix/sig_init.c sig_init.c.patch
patch ../sr_unix/gtm_dump_core.c gtm_dump_core.c.patch
patch ../sr_unix/gtm_fork_n_core.c gtm_fork_n_core.c.patch

mkdir build-instrumented
echo "Cmaking..."
cd build-instrumented || exit
CC=$(which afl-gcc-fast) CXX=$(which afl-g++-fast) cmake ../../
echo "Making..."
make -j "$(nproc)"
