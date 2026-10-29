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

patch:
	patch --no-backup-if-mismatch ../sr_port/io.h io.h.patch
	patch --no-backup-if-mismatch ../sr_port/jobexam_process.c jobexam_process.c.patch
	patch --no-backup-if-mismatch ../sr_port/op_fnzsigproc.c op_fnzsigproc.c.patch
	patch --no-backup-if-mismatch ../sr_port/op_hang.c op_hang.c.patch
	patch --no-backup-if-mismatch ../sr_port/op_zsystem.c op_zsystem.c.patch
	patch --no-backup-if-mismatch ../sr_port/stp_gcol_src.h stp_gcol_src.h.patch
	patch --no-backup-if-mismatch ../sr_unix/gtm_dump_core.c gtm_dump_core.c.patch
	patch --no-backup-if-mismatch ../sr_unix/gtm_fork_n_core.c gtm_fork_n_core.c.patch
	patch --no-backup-if-mismatch ../sr_unix/gtm_putmsg_list.c gtm_putmsg_list.c.patch
	patch --no-backup-if-mismatch ../sr_unix/iopi_open.c iopi_open.c.patch
	patch --no-backup-if-mismatch ../sr_unix/jobchild_init.c jobchild_init.c.patch
	patch --no-backup-if-mismatch ../sr_unix/ojchildioset.c ojchildioset.c.patch
	patch --no-backup-if-mismatch ../sr_unix/op_fnzsyslog.c op_fnzsyslog.c.patch
	patch --no-backup-if-mismatch ../sr_unix/op_zmess.c op_zmess.c.patch
	patch --no-backup-if-mismatch ../sr_unix/op_zedit.c op_zedit.c.patch
	patch --no-backup-if-mismatch ../sr_unix/sig_init.c sig_init.c.patch

reset:
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_port/io.h io.h.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_port/jobexam_process.c jobexam_process.c.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_port/op_fnzsigproc.c op_fnzsigproc.c.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_port/op_hang.c op_hang.c.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_port/op_zsystem.c op_zsystem.c.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_port/stp_gcol_src.h stp_gcol_src.h.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_unix/gtm_dump_core.c gtm_dump_core.c.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_unix/gtm_fork_n_core.c gtm_fork_n_core.c.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_unix/gtm_putmsg_list.c gtm_putmsg_list.c.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_unix/iopi_open.c iopi_open.c.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_unix/jobchild_init.c jobchild_init.c.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_unix/ojchildioset.c ojchildioset.c.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_unix/op_fnzsyslog.c op_fnzsyslog.c.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_unix/op_zmess.c op_zmess.c.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_unix/op_zedit.c op_zedit.c.patch
	patch --reject-file=- --no-backup-if-mismatch --reverse ../sr_unix/sig_init.c sig_init.c.patch

clean: rm reset

rm:
	rm -rf build-instrumented inputs output env NotMinCorpus YDBTest

