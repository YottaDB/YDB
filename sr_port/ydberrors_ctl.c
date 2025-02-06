/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries.*
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
	{ "QUERY2", "Invalid second argument to $QUERY. Must be -1 or 1.", 0, 0 },
	{ "MIXIMAGE", "Cannot load !AD image on process that already has !AD image loaded", 4, 0 },
	{ "LIBYOTTAMISMTCH", "$ydb_dist/libyottadb.so (!AD) does not match the shared library path (!AD)", 4, 0 },
	{ "READONLYNOSTATS", "Read-only and Statistics sharing cannot both be enabled on database", 0, 0 },
	{ "READONLYLKFAIL", "Failed to get !AD lock on READ_ONLY database file !AD", 4, 0 },
	{ "INVVARNAME", "Invalid local/global/ISV variable name !AD supplied to API call", 2, 0 },
	{ "PARAMINVALID", "!AD parameter specified in !AD call", 4, 0 },
	{ "INSUFFSUBS", "Return subscript array for !AD call too small - needs at least !UL entries for this call", 3, 0 },
	{ "MINNRSUBSCRIPTS", "Number of subscripts cannot be a negative number", 0, 0 },
	{ "SUBSARRAYNULL", "Non-zero number of subscripts [!UL] specified but subscript array parameter is NULL in !AD call", 3, 0 },
	{ "FATALERROR1", "Fatal error raised. Generating core and terminating process. Error: !AD", 2, 0 },
	{ "NAMECOUNT2HI", "Number of varnames specified as the namecount parameter in a !AD call (!UL) exceeds the maximum (!UL)", 4, 0 },
	{ "INVNAMECOUNT", "Number of varnames (namecount parameter in a !AD call) cannot be less than zero", 2, 0 },
	{ "FATALERROR2", "Fatal error raised. Bypassing core generation and terminating process. Error: !AD", 2, 0 },
	{ "TIME2LONG", "Specified time value [0x!16@XQ] exceeds supported maximum [0x!16@XQ]", 2, 0 },
	{ "VARNAME2LONG", "Variable name length exceeds maximum allowed (!UL)", 1, 0 },
	{ "SIMPLEAPINEST", "Attempt to nest call of !AZ with a call to !AZ - nesting calls is not permitted in the Simple API", 2, 0 },
	{ "CALLINTCOMMIT", "TCOMMIT at call-in-level=!UL not allowed as corresponding TSTART was done at lower call-in-level=!UL", 2, 0 },
	{ "CALLINTROLLBACK", "TROLLBACK at call-in-level=!UL not allowed as corresponding TSTART was done at lower call-in-level=!UL", 2, 0 },
	{ "TCPCONNTIMEOUT", "Connection wait timeout (!UL seconds) has expired", 2, 0 },
	{ "STDERRALREADYOPEN", "STDERR deviceparameter specifies an already open device !AD", 2, 0 },
	{ "SETENVFAIL", "VIEW \"SETENV\":\"!AD\" failed in setenv() system call", 2, 0 },
	{ "UNSETENVFAIL", "VIEW \"UNSETENV\":\"!AD\" failed in unsetenv() system call", 2, 0 },
	{ "UNKNOWNSYSERR", "[!UL] does not correspond to a known YottaDB error code", 1, 0 },
	{ "READLINEFILEPERM", "Readline history file !AZ could not be created", 1, 0 },
	{ "NODEEND", "End of list of nodes/subscripts", 0, 0 },
	{ "READLINELONGLINE", "Entered line is greater than 32Kb long, exceeding maximum allowed", 0, 0 },
	{ "INVTPTRANS", "Invalid TP transaction - either invalid TP token or transaction not in progress", 0, 0 },
	{ "THREADEDAPINOTALLOWED", "Process cannot switch to using threaded Simple API while already using Simple API", 0, 0 },
	{ "SIMPLEAPINOTALLOWED", "Process cannot switch to using Simple API while already using threaded Simple API", 0, 0 },
	{ "STAPIFORKEXEC", "Calls to YottaDB are not supported after a fork() if threaded Simple API functions were in use in parent. Call exec() first", 0, 0 },
	{ "INVVALUE", "!AD is invalid !AD value for !AD", 6, 0 },
	{ "INVZCONVERT", "Translation supported only between DEC/HEX OR between UTF-8/UTF-16/UTF-16LE/UTF-16BE", 0, 0 },
	{ "ZYSQLNULLNOTVALID", "$ZYSQLNULL cannot be used as an integer, numeric, gvn subscript/value or lock subscript", 0, 0 },
	{ "BOOLEXPRTOODEEP", "Boolean expression depth exceeds maximum supported limit of 2047", 0, 0 },
	{ "TPCALLBACKINVRETVAL", "Invalid return type for TP callback function", 0, 0 },
	{ "INVMAINLANG", "Invalid main routine language id specified: !UL", 1, 0 },
	{ "WCSFLUFAILED", "!AD error while flushing buffers at transaction number 0x!16@XQ for database file !AD", 5, 0 },
	{ "WORDEXPFAILED", "wordexp() call for string [!AD] returned !AZ error. See wordexp() man pages for details", 3, 0 },
	{ "TRANSREPLJNL1GB", "Transaction can use at most 1GiB of replicated journal records across all journaled regions", 0, 0 },
	{ "DEVPARPARSE", "Error parsing device parameter specification", 0, 0 },
	{ "SETZDIRTOOLONG", "$ZDIR value specified is !UL bytes long which is greater than the allowed maximum of !UL bytes", 2, 0 },
	{ "UTF8NOTINSTALLED", "$ydb_dist does not have utf8 folder installed. Please use M mode or re-install YottaDB with UTF-8 support", 0, 0 },
	{ "ISVUNSUPPORTED", "ISV variable name !AD not supported in !AD call", 4, 0 },
	{ "GVNUNSUPPORTED", "Global variable name !AD not supported in !AD call", 4, 0 },
	{ "ISVSUBSCRIPTED", "ISV variable name !AD specified with a non-zero subscript count of !UL", 3, 0 },
	{ "ZBRKCNTNEGATIVE", "Count [!SL], of transits through a ZBREAK breakpoint before activating it, cannot be negative", 1, 0 },
	{ "SECSHRPATHMAX", "gtmsecshr executable path length is greater than maximum (!UL)", 1, 0 },
	{ "MUTRUNCALREADY", "Region !AD: no further truncation possible", 2, 0 },
	{ "ARGSLONGLINE", "Entered line is greater than maximum characters allowed (!UL)", 1, 0 },
	{ "ZGBLDIRUNDEF", "Global Directory env var $ydb_gbldir/$gtmgbldir is undefined", 0, 0 },
	{ "SHEBANGMEXT", "!AZ needs a .m extension to be a valid shebang script", 1, 0 },
	{ "ZCPREALLVALSTR", "Pre-allocation allowed only for output or input/output variables of type ydb_buffer_t*, ydb_string_t*, or ydb_char_t*", 0, 0 },
	{ "ZSHOWSTACKRANGE", "Invalid stack level value !SL for ZSHOW \"V\"", 1, 0 },
	{ "GVDBGNAKEDUNSET", "Invalid GVNAKED in gv_optimize: $REFERENCE was unset. Opcodes seen: !AD", 2, 0 },
	{ "GVDBGNAKEDMISMATCH", "Invalid GVNAKED in gv_optimize: $REFERENCE did not match OP_GVNAKED: !AD !!= !AD. Opcodes seen: !AD", 6, 0 },
};



LITDEF	int ydberrors_undocarr[] = {
	0	/* Placeholder to prevent empty array */
};


GBLDEF	err_ctl ydberrors_ctl = {
	256,
	"YDB",
	&ydberrors[0],
	56,
	&ydberrors_undocarr[0],
	0
};

