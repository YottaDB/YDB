/****************************************************************
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
JOBERR_TABLE_ENTRY(joberr_enum,     joberr_string)
*/

JOBERR_TABLE_ENTRY (joberr_ok,                   "")
JOBERR_TABLE_ENTRY (joberr_gen,                  "Job error in child process")
JOBERR_TABLE_ENTRY (joberr_io_stdin_open,        "Job error in opening STDIN")
JOBERR_TABLE_ENTRY (joberr_io_stdin_dup,         "Job error in directing input to STDIN")
JOBERR_TABLE_ENTRY (joberr_io_stdout_creat,      "Job error in creating STDOUT")
JOBERR_TABLE_ENTRY (joberr_io_stdout_open,       "Job error in opening STDOUT")
JOBERR_TABLE_ENTRY (joberr_io_stdout_dup,        "Job error in directing output to STDOUT")
JOBERR_TABLE_ENTRY (joberr_io_stderr_creat,      "Job error in creating STDERR")
JOBERR_TABLE_ENTRY (joberr_io_stderr_open,       "Job error in opening STDERR")
JOBERR_TABLE_ENTRY (joberr_io_stderr_dup,        "Job error in directing output to STDERR")
JOBERR_TABLE_ENTRY (joberr_cd_toolong,           "Job error in directory specification")
JOBERR_TABLE_ENTRY (joberr_cd,                   "Job error - CHDIR error")
JOBERR_TABLE_ENTRY (joberr_rtn,                  "Job error in routine or label or offset specification.")
JOBERR_TABLE_ENTRY (joberr_sid,                  "Job error in setting independent session")
JOBERR_TABLE_ENTRY (joberr_sp,                   "Job error in socketpair")
JOBERR_TABLE_ENTRY (joberr_frk,                  "Job error in fork")
JOBERR_TABLE_ENTRY (joberr_stdout_rename,        "Job error in renaming standard output file")
JOBERR_TABLE_ENTRY (joberr_stderr_rename,        "Job error in renaming standard error file")
JOBERR_TABLE_ENTRY (joberr_pipe_mp,              "Job error in middle process to parent process pipe communication")
JOBERR_TABLE_ENTRY (joberr_pipe_mgc,             "Job error in middle process to grandchild process pipe communication")
JOBERR_TABLE_ENTRY (joberr_stdin_socket_lookup,  "Job error - INPUT socket not found in socket pool")
JOBERR_TABLE_ENTRY (joberr_stdout_socket_lookup, "Job error - OUTPUT socket not found in socket pool")
JOBERR_TABLE_ENTRY (joberr_stderr_socket_lookup, "Job error - ERROR socket not found in socket pool")
JOBERR_TABLE_ENTRY (joberr_io_stdin_socket_dup,  "Job error in copying INPUT socket descriptor")
JOBERR_TABLE_ENTRY (joberr_io_stdout_socket_dup, "Job error in copying OUTPUT socket descriptor")
JOBERR_TABLE_ENTRY (joberr_io_stderr_socket_dup, "Job error in copying ERROR socket descriptor")
JOBERR_TABLE_ENTRY (joberr_io_setup_op_write,    "Job error sending setup command")
JOBERR_TABLE_ENTRY (joberr_io_setup_write,       "Job error sending setup data")
JOBERR_TABLE_ENTRY (joberr_sig,                  "Job child terminated due to signal")
JOBERR_TABLE_ENTRY (joberr_stp,                  "Job child was stopped by signal")	/* These two should stay at end of enum */
JOBERR_TABLE_ENTRY (joberr_end,                  "")

