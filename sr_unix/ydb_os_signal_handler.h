/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef YDB_OS_SIGNAL_HANDLER_INCLUDED
#define YDB_OS_SIGNAL_HANDLER_INCLUDED

void ydb_os_signal_handler(int sig, siginfo_t *info, void *context);

#endif /* YDB_OS_SIGNAL_HANDLER_INCLUDED */
