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

#include "mdef.h"

#include "sig_init.h"
#include "libyottadb.h"

/* The alternate handler for SIGUSR1 - we are called from the dispatcher and call the regular handler but with two
 * NULL parms that are not used when doing alternate signal handling.
 */
int ydb_altusr1_sighandler(int signum)
{
	null_handler(signum, NULL, NULL);
	return YDB_OK;
}
