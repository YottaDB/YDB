/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries.*
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
	{ "FATALERROR1", "Fatal error raised. Generating core and terminating process. Error: !AD", 2 },
	{ "NAMECOUNT2HI", "Number of varnames specified as the namecount parameter in a !AD call (!UL) exceeds the maximum (!UL)", 4 },
	{ "INVNAMECOUNT", "Number of varnames (namecount parameter in a !AD call) cannot be less than zero", 2 },
	{ "FATALERROR2", "Fatal error raised. Bypassing core generation and terminating process. Error: !AD", 2 },
	{ "TIME2LONG", "Specified time value [0x!16@XQ] exceeds supported maximum [0x!16@XQ]", 2 },
	{ "VARNAME2LONG", "Variable name length exceeds maximum allowed (!UL)", 1 },
	{ "SIMPLEAPINEST", "Attempt to nest call of !AZ with a call to !AZ - nesting calls is not permitted in the Simple API", 2 },
	{ "CALLINTCOMMIT", "TCOMMIT at call-in-level=!UL not allowed as corresponding TSTART was done at lower call-in-level=!UL", 2 },
	{ "CALLINTROLLBACK", "TROLLBACK at call-in-level=!UL not allowed as corresponding TSTART was done at lower call-in-level=!UL", 2 },
	{ "TCPCONNTIMEOUT", "Connection wait timeout (!UL seconds) has expired", 2 },
	{ "STDERRALREADYOPEN", "STDERR deviceparameter specifies an already open device !AD", 2 },
	{ "SETENVFAIL", "VIEW \"SETENV\":\"!AD\" failed in setenv() system call", 2 },
	{ "UNSETENVFAIL", "VIEW \"UNSETENV\":\"!AD\" failed in unsetenv() system call", 2 },
	{ "UNKNOWNSYSERR", "[!UL] does not correspond to a known YottaDB error code", 1 },
	{ "STRUCTNOTALLOCD", "Structure not previously called with Alloc() method", 0 },
	{ "NODEEND", "End of list of nodes/subscripts", 0 },
	{ "INVLNPAIRLIST", "Invalid lockname/subscript pair list (uneven number of lockname/subscript parameters)", 0 },
	{ "INVTPTRANS", "Invalid TP transaction - either invalid TP token or transaction not in progress", 0 },
	{ "THREADEDAPINOTALLOWED", "Process cannot switch to using threaded Simple API while already using Simple API", 0 },
	{ "SIMPLEAPINOTALLOWED", "Process cannot switch to using Simple API while already using threaded Simple API", 0 },
	{ "STAPIFORKEXEC", "Calls to YottaDB are not supported after a fork() if threaded Simple API functions were in use in parent. Call exec() first", 0 },
	{ "INVVALUE", "Invalid !AD value for !AD : !AD ", 6 },
};



LITDEF	int ydberrors_undocarr[] = {
};


GBLDEF	err_ctl ydberrors_ctl = {
	256,
	"YDB",
	&ydberrors[0],
	32,
	&ydberrors_undocarr[0],
	0
};

