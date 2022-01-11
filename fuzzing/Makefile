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

all: instrument corpus go

instrument:
	./instrument.sh
	echo "Instrumented..."

corpus:
	./make_corpus.sh
	echo "Corpus created..."

go:
	./fuzz.sh
	echo "working on it"

reset:
	patch -R ../sr_port/io.h io.h.patch
	patch -R ../sr_port/jobexam_process.c jobexam_process.c.patch
	patch -R ../sr_unix/ojchildioset.c ojchildioset.c.patch
	patch -R ../sr_unix/sig_init.c sig_init.c.patch
	patch -R ../sr_unix/gtm_dump_core.c gtm_dump_core.c.patch
	patch -R ../sr_unix/gtm_fork_n_core.c gtm_fork_n_core.c.patch

clean: rm reset

rm:
	rm -rf build-instrumented inputs env YDBTest
