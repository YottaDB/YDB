/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"

LITDEF	err_msg ydberrors[] = {
	{ "QUERY2", "Invalid second argument to $QUERY. Must be -1 or 1.", 0 },
	{ "MIXIMAGE", "Cannot load !AD image on process that already has !AD image loaded", 4 },
	{ "LIBYOTTAMISMTCH", "$ydb_dist/libyottadb.so (!AD) does not match the shared library path (!AD)", 4 },
	{ "READONLYNOSTATS", "Read-only and Statistics sharing cannot both be enabled on database", 0 },
	{ "READONLYLKFAIL", "Failed to get !AD lock on READ_ONLY database file !AD", 4 },
	{ "VARNAMEINVALID", "Invalid local/global/ISV variable name supplied to API call", 0 },
	{ "VALUEINVALID", "Invalid value specified to ydb_set_s() call", 0 },
	{ "NORETBUFFER", "No return/output buffer provided for !AD call", 2 },
};


GBLDEF	err_ctl ydberrors_ctl = {
	256,
	"YDB",
	&ydberrors[0],
	8};
