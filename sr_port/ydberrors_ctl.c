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
	{ "INVVARNAME", "Invalid local/global/ISV variable name supplied to API call", 0 },
	{ "PARAMINVALID", "!AD parameter specified in !AD call", 4 },
	{ "INSUFFSUBS", "Return subscript array for !AD call too small - needs at least !UL entries for this call", 3 },
	{ "MINNRSUBSCRIPTS", "Number of subscripts cannot be a negative number", 0 },
	{ "SUBSARRAYNULL", "Non-zero number of subscripts [!UL] specified but subscript array parameter is NULL in !AD call", 3 },
	{ "MISSINGVARNAMES", "At least one variable name must be specified", 0 },
	{ "TOOMANYVARNAMES", "Number of varnames specified exceeds maximum (!UL)", 1 },
	{ "INVNAMECOUNT", "namecount parameter in a !AD call cannot be a negative number", 2 },
	{ "MISSINGVARNAME", "Required varname not specified", 0 },
	{ "TIME2LONG", "Specified time value [0x!16@XQ] exceeds supported maximum [0x!16@XQ]", 2 },
	{ "VARNAME2LONG", "Variable name length exceeds maximum allowed (!UL)", 1 },
	{ "FATALERROR1", "Fatal error raised. Generating core and terminating process. Error: !AD", 2 },
	{ "FATALERROR2", "Fatal error raised. Bypassing core generation and terminating process. Error: !AD", 2 },
	{ "SIMPLEAPINEST", "Attempt to nest call of !AD with a call to !AD - nesting calls is not permitted in simpleAPI", 4 },
	{ "CALLINTCOMMIT", "TCOMMIT at call-in-level=!UL not allowed as corresponding TSTART was done at lower call-in-level=!UL", 2 },
	{ "CALLINTROLLBACK", "TROLLBACK at call-in-level=!UL not allowed as corresponding TSTART was done at lower call-in-level=!UL", 2 },
};


GBLDEF	err_ctl ydberrors_ctl = {
	256,
	"YDB",
	&ydberrors[0],
	21};
