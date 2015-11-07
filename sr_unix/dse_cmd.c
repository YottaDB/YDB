/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab_int4.h"     /* needed for tp.h */
#include "buddy_list.h"
#include "tp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "util.h"
#include "cli.h"
#include "cache.h"
#include "op.h"
#include "gt_timer.h"
#include "io.h"
#include "dse.h"
#include "compiler.h"

#include "dse_exit.h"
#include "util_spawn.h"
#include "util_help.h"
#include "dse_cmd_disallow.h"

/*************************************************************
 * NOTE
 * This file might have lines longer than 132 characters
 * since the command tables are being initialized.
 *
 * Entries need to be made in sorted order (lexicographic) within
 * each table.
 ************************************************************/

static readonly CLI_PARM dse_ftime_parm_values[] = {
	{ "FLUSH_TIME", 1, PARM_REQ}
};

static readonly CLI_ENTRY dse_add_qual[] = {
{ "BLOCK",   0,          0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "DATA",    0,          0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_STR, 0       },
{ "KEY",     0,          0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_STR, 0       },
{ "OFFSET",  0,          0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "POINTER", 0,          0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "RECORD",  0,          0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "STAR",    dse_adstar, 0, 0, 0, 0, 0, VAL_N_A, 0, 0,       0,       0       },
{ 0 }
};

static readonly CLI_ENTRY dse_cache_qual[] = {
{ "ALL",     0, 0, 0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ "CHANGE",  0, 0, 0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ "CRIT",    0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG,     0,       0       },
{ "OFFSET",  0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "RECOVER", 0, 0, 0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ "SHOW",    0, 0, 0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ "SIZE",    0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_DCM },
{ "VALUE",   0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "VERIFY",  0, 0, 0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ 0 }
};

static readonly CLI_ENTRY dse_all_qual[] = {
{ "ALL",          0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0,   0, 0 },
{ "BUFFER_FLUSH", 0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0,   0, 0 },
{ "CRITINIT",     0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0,   0, 0 },
{ "DUMP",         0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0,   0, 0 },
{ "FREEZE",       0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "OVERRIDE",     0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0,   0, 0 },
{ "REFERENCE",    0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0,   0, 0 },
{ "RELEASE",      0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0,   0, 0 },
{ "RENEW",        0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0,   0, 0 },
{ "SEIZE",        0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0,   0, 0 },
{ "WCINIT",       0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0,   0, 0 },
{ 0 }
};

static readonly CLI_ENTRY true_false_nochange[] = {
	{ "FALSE",	0, 0, 0, 0, 0, 0, VAL_DISALLOWED,	0,	NEG,	VAL_N_A,	 0 },
	{ "NOCHANGE",	0, 0, 0, 0, 0, DEFA_PRESENT, VAL_DISALLOWED,	0,	NEG,	VAL_N_A, 0 },
	{ "TRUE",	0, 0, 0, 0, 0, 0, VAL_DISALLOWED,	0,	NEG,	VAL_N_A,	 0 },
	{ 0, 0, 0 }
};

static readonly CLI_ENTRY never_always_allowexisting[] = {
	{ "ALWAYS",	0, 0, 0, 0, 0, 0, VAL_DISALLOWED,	0,	NON_NEG,	VAL_STR,	 0 },
	{ "EXISTING",	0, 0, 0, 0, 0, 0, VAL_DISALLOWED,	0,	NON_NEG,	VAL_STR,	 0 },
	{ "FALSE",	0, 0, 0, 0, 0, 0, VAL_DISALLOWED,	0,	NON_NEG,	VAL_STR,	 0 },
	{ "NEVER",	0, 0, 0, 0, 0, 0, VAL_DISALLOWED,	0,	NON_NEG,	VAL_STR,	 0 },
	{ "TRUE",	0, 0, 0, 0, 0, 0, VAL_DISALLOWED,	0,	NON_NEG,	VAL_STR,	 0 },
	{ 0, 0, 0 }
};

static readonly CLI_ENTRY db_vers[] = {
	{ "V4",	0, 0, 0, 0, 0, 0, VAL_DISALLOWED,	0,	NON_NEG,	VAL_N_A,	 0 },
	{ "V6",	0, 0, 0, 0, 0, 0, VAL_DISALLOWED,	0,	NON_NEG,	VAL_N_A, 	 0 },
};

static readonly CLI_ENTRY dse_cfhead_qual[] = {
{ "ABANDONED_KILLS",           0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "AVG_BLKS_READ",             0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "BLKS_TO_UPGRADE",           0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "BLK_SIZE",                  0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "BLOCKS_FREE",               0, 0, 0,                     0, 			 0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "B_BYTESTREAM",              0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "B_COMPREHENSIVE",           0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "B_DATABASE",                0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "B_INCREMENTAL",             0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "B_RECORD",                  0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "CERT_DB_VER",               0, 0, 0,                     db_vers,             0, 0, VAL_REQ,     0, NON_NEG, VAL_STR,  0       },
{ "COMMITWAIT_SPIN_COUNT",     0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "CORRUPT_FILE",              0, 0, 0,                     true_false_nochange, 0, 0, VAL_REQ,     0, NON_NEG, VAL_STR,  0       },
{ "CRIT",                      0, 0, 0,                     0,                   0, 0, VAL_N_A,     0, NEG,     0,        0       },
{ "CURRENT_TN",                0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "DB_WRITE_FMT",              0, 0, 0,                     db_vers,             0, 0, VAL_REQ,     0, NON_NEG, VAL_STR,  0       },
{ "DECLOCATION",               0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "DECVALUE",                  0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "DEF_COLLATION",             0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "DUALSITE_RESYNC_SEQNO",     0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "ENCRYPTION_HASH",           0, 0, 0,                     0,                   0, 0, VAL_N_A,	    0, NON_NEG, 0,        0       },
{ "FLUSH_TIME",                0, 0, dse_ftime_parm_values, 0,                   0, 0, VAL_NOT_REQ, 0, NON_NEG, VAL_TIME, 0       },
{ "FREEZE",                    0, 0, 0,                     true_false_nochange, 0, 0, VAL_REQ,     0, NON_NEG, VAL_STR,  0       },
{ "FULLY_UPGRADED",            0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "GOT2V5ONCE",                0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "GVSTATSRESET",              0, 0, 0,                     0,                   0, 0, VAL_N_A,     0, NON_NEG, 0,        0       },
{ "HARD_SPIN_COUNT",           0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "HEXLOCATION",               0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "HEXVALUE",                  0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "INHIBIT_KILLS",             0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "INTERRUPTED_RECOV",         0, 0, 0,                     true_false_nochange, 0, 0, VAL_REQ,     0, NON_NEG, VAL_STR,  0       },
{ "JNL_SYNCIO",                0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_STR,  0       },
{ "JNL_YIELD_LIMIT",           0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "KEY_MAX_SIZE",              0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "KILL_IN_PROG",              0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "LOCATION",                  0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "MACHINE_NAME",              0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_STR,  0       },
{ "MAX_TN",                    0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "MBM_SIZE",                  0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
/* Maintain MUTEX_* qualifiers for backward compatibility; remove these entries after sufficient time has passed for users to have
 * made the switch to the synonymn qualifiers that don't have the MUTEX_ prefix.
 */
{ "MUTEX_HARD_SPIN_COUNT",     0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "MUTEX_SLEEP_SPIN_COUNT",    0, 0, 0,                     0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "MUTEX_SPIN_SLEEP_TIME",     0, 0, 0,                     0, 		         0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
/* End MUTEX_* qualifiers */
{ "NULL_SUBSCRIPTS",           0, 0, 0,                     never_always_allowexisting, 0, 0, VAL_REQ, 0, NON_NEG, VAL_STR, 0     },
{ "ONLINE_NBB",                0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_STR,  0       },
{ "OVERRIDE",                  0, 0, 0,			    0,                   0, 0, VAL_N_A,     0, 0,       0,        0       },
{ "PRE_READ_TRIGGER_FACTOR",   0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "QDBRUNDOWN",                0, 0, 0,                     0,                   0, 0, VAL_N_A,     0, NEG,     0,        0       },
{ "RC_SRV_COUNT",              0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "RECORD_MAX_SIZE",           0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "REFERENCE_COUNT",           0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "REG_SEQNO",                 0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "RESERVED_BYTES",            0, 0, 0, 	   	    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "SIZE",                      0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "SLEEP_SPIN_COUNT",          0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "SPIN_SLEEP_TIME",           0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "STDNULLCOLL",               0, 0, 0, 		    true_false_nochange, 0, 0, VAL_REQ,     0, NON_NEG, VAL_STR,  0       },
{ "STRM_NUM",                  0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "STRM_REG_SEQNO",            0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "TIMERS_PENDING",            0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "TOTAL_BLKS",                0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "TRIGGER_FLUSH",             0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "UPD_RESERVED_AREA",         0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "UPD_WRITER_TRIGGER_FACTOR", 0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "VALUE",                     0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "WAIT_DISK",                 0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "WARN_MAX_TN",               0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "WRITES_PER_FLUSH",          0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_DCM },
{ "ZQGBLMOD_SEQNO",            0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ "ZQGBLMOD_TN",               0, 0, 0, 		    0,                   0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM,  VAL_HEX },
{ 0 }
};

static readonly CLI_ENTRY dse_change_qual[] = {
{ "BLOCK",      0,              0,               0, 0, 0,                           0, VAL_REQ,        0, NON_NEG, VAL_NUM, VAL_HEX },
{ "BSIZ",       0,              0,               0, 0, 0,                           0, VAL_REQ,        0, NON_NEG, VAL_NUM, VAL_HEX },
{ "CMPC",       dse_chng_rhead, 0,               0, 0, 0,                           0, VAL_REQ,        0, NON_NEG, VAL_NUM, VAL_HEX },
{ "FILEHEADER", dse_chng_fhead, dse_cfhead_qual, 0, 0, cli_disallow_dse_chng_fhead, 0, VAL_DISALLOWED, 0, 0,       0,       0       },
{ "LEVEL",      0,              0,               0, 0, 0,                           0, VAL_REQ,        0, NON_NEG, VAL_NUM, VAL_HEX },
{ "OFFSET",     dse_chng_rhead, 0,               0, 0, 0,                           0, VAL_REQ,        0, NON_NEG, VAL_NUM, VAL_HEX },
{ "RECORD",     dse_chng_rhead, 0,               0, 0, 0,                           0, VAL_REQ,        0, NON_NEG, VAL_NUM, VAL_HEX },
{ "RSIZ",       dse_chng_rhead, 0,               0, 0, 0,                           0, VAL_REQ,        0, NON_NEG, VAL_NUM, VAL_HEX },
{ "TN",         0,              0,               0, 0, 0,                           0, VAL_REQ,        0, NON_NEG, VAL_NUM, VAL_HEX },
{ 0 }
};

static readonly CLI_ENTRY dse_crit_qual[] = {
{ "ALL",     0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0, 0, 0 },
{ "CRASH",   0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0, 0, 0 },
{ "CYCLE",   0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0, 0, 0 },
{ "INIT",    0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0, 0, 0 },
{ "OWNER",   0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0, 0, 0 },
{ "RELEASE", 0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0, 0, 0 },
{ "REMOVE",  0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0, 0, 0 },
{ "RESET",   0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0, 0, 0 },
{ "SEIZE",   0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0, 0, 0 },
{ 0 }
};

static readonly CLI_ENTRY dse_rdmp_qual[] = {
{ "BLOCK",  0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "COUNT",  0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "GLO",    0, 0, 0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ "OFFSET", 0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "RECORD", 0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "ZWR",    0, 0, 0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ 0 }
};

static readonly CLI_ENTRY dse_fdmp_qual[] = {
{ "ALL",           0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0,   0, 0 },
{ "BACKUP",        0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "BASIC",         0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "BG_TRC",        0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "DB_CSH",        0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "ENVIRONMENT",   0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "GVSTATS",       0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "JOURNAL",       0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "MIXEDMODE",     0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "RETRIES",       0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "SNAPSHOT",      0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "SUPPLEMENTARY", 0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "TPBLKMOD",      0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "TPRETRIES",     0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ "UPDPROC",       0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG, 0, 0 },
{ 0 }
};

static readonly CLI_ENTRY dse_dump_qual[] = {
{ "BLOCK",      0,             0,             0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "COUNT",      0,             0,             0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "CRIT",       0,             0,             0, 0, 0, 0, VAL_N_A, 0, NEG,     0,       0       },
{ "FILEHEADER", dse_dmp_fhead, dse_fdmp_qual, 0, 0, 0, 0, VAL_N_A, 0, 0,       0,       0       },
{ "GLO",        0,             0,             0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ "HEADER",     0,             0,             0, 0, 0, 0, VAL_N_A, 0, NEG,     0,       0       },
{ "OFFSET",     0,             0,             0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "RECORD",     0,             0,             0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "ZWR",        0,             0,             0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ 0 }
};

static readonly CLI_ENTRY dse_eval_qual[] = {
{ "DECIMAL",     0, 0, 0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ "HEXADECIMAL", 0, 0, 0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ "NUMBER",      0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ 0 }
};

static readonly CLI_PARM dse_freg_parm_values[] = {
	{ "REGION", "*", PARM_REQ}
};

static readonly CLI_ENTRY dse_find_qual[] = {
{ "BLOCK",      0,          0, 0,                    0, 0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM, VAL_HEX },
{ "CRIT",       0,          0, 0,                    0, 0, 0, VAL_N_A,     0, NEG,     0,       0       },
{ "EXHAUSTIVE", 0,          0, 0,                    0, 0, 0, VAL_N_A,     0, NON_NEG, 0,       0       },
{ "FREEBLOCK",  dse_f_free, 0, 0,                    0, 0, 0, VAL_N_A,     0, NON_NEG, 0,       0       },
{ "HINT",       0,          0, 0,                    0, 0, 0, VAL_REQ,     0, NON_NEG, VAL_NUM, VAL_HEX },
{ "KEY",        dse_f_key,  0, 0,                    0, 0, 0, VAL_REQ,     0, NON_NEG, VAL_STR, 0       },
{ "REGION",     dse_f_reg,  0, dse_freg_parm_values, 0, 0, 0, VAL_NOT_REQ, 0, NON_NEG, VAL_STR, 0       },
{ "SIBLINGS",   0,          0, 0,                    0, 0, 0, VAL_N_A,     0, NON_NEG, 0,       0       },
{ 0 }
};

static readonly CLI_ENTRY dse_integrit_qual[] = {
{ "BLOCK",   0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "CRIT",    0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG,     0,       0       },
{ 0 }
};

static readonly CLI_ENTRY dse_map_qual[] = {
{ "BLOCK",       0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "BUSY",        0, 0, 0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ "FREE",        0, 0, 0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ "MASTER",      0, 0, 0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ "RESTORE_ALL", 0, 0, 0, 0, 0, 0, VAL_N_A, 0, NON_NEG, 0,       0       },
{ 0 }
};

static readonly CLI_ENTRY dse_open_qual[] = {
{ "FILE", 0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_STR, 0   },
{ "OCHSET", 0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_STR, 0 },
{ 0 }
};


static readonly CLI_ENTRY dse_over_qual[] = {
{ "BLOCK",  0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "DATA",   0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_STR, 0       },
{ "OFFSET", 0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "OCHSET", 0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, 0       },
{ 0 }
};

static readonly CLI_ENTRY dse_range_qual[] = {
{ "BUSY",  0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NEG,	VAL_N_A, 0	 },
{ "CRIT",  0, 0, 0, 0, 0, 0, VAL_N_A, 	    0, NEG, 	0,       0       },
{ "FROM",  0, 0, 0, 0, 0, 0, VAL_REQ,	    0, NON_NEG,	VAL_NUM, VAL_HEX },
{ "INDEX", 0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG,	VAL_N_A, 0	 },
{ "LOST",  0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG,	VAL_N_A, 0	 },
{ "LOWER", 0, 0, 0, 0, 0, 0, VAL_REQ,	    0, NON_NEG,	VAL_STR, 0	 },
{ "STAR",  0, 0, 0, 0, 0, 0, VAL_DISALLOWED, 0, NON_NEG,	VAL_N_A, 0	 },
{ "TO",	   0, 0, 0, 0, 0, 0, VAL_REQ,	    0, NON_NEG,	VAL_NUM, VAL_HEX },
{ "UPPER", 0, 0, 0, 0, 0, 0, VAL_REQ,	    0, NON_NEG,	VAL_STR, 0	 },
{ 0 }
};

static readonly CLI_ENTRY dse_remove_qual[] = {
{ "BLOCK",   0,         0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "COUNT",   0,         0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "OFFSET",  dse_rmrec, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "RECORD",  dse_rmrec, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "VERSION", 0,         0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_DCM },
{ 0 }
};

static readonly CLI_ENTRY dse_restore_qual[] = {
{ "BLOCK",   0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "FROM",    0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "REGION",  0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_STR, 0       },
{ "VERSION", 0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_DCM },
{ 0 }
};

static readonly CLI_ENTRY dse_save_qual[] = {
{ "BLOCK",   0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "COMMENT", 0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_STR, 0       },
{ "CRIT",    0, 0, 0, 0, 0, 0, VAL_N_A, 0, NEG,     0,       0       },
{ "LIST",    0, 0, 0, 0, 0, 0, VAL_N_A, 0, 0,       0,       0       },
{ 0 }
};

static readonly CLI_ENTRY dse_shift_qual[] = {
{ "BACKWARD", 0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "FORWARD",  0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ "OFFSET",   0, 0, 0, 0, 0, 0, VAL_REQ, 0, NON_NEG, VAL_NUM, VAL_HEX },
{ 0 }
};

GBLDEF CLI_ENTRY dse_cmd_ary[] = {
{ "ADD",          dse_adrec,      dse_add_qual,      0, 0, cli_disallow_dse_add,    0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "ALL",          dse_all,        dse_all_qual,      0, 0, cli_disallow_dse_all,    0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "BUFFER_FLUSH", dse_flush,      0,                 0, 0, 0,                       0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "CACHE",        dse_cache,      dse_cache_qual,    0, 0, cli_disallow_dse_cache,  0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "CHANGE",       dse_chng_bhead, dse_change_qual,   0, 0, cli_disallow_dse_change, 0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "CLOSE",        dse_close,      0,                 0, 0, 0,                       0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "CRITICAL",     dse_crit,       dse_crit_qual,     0, 0, cli_disallow_dse_crit,   0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "DUMP",         dse_dmp,        dse_dump_qual,     0, 0, cli_disallow_dse_dump,   0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "EVALUATE",     dse_eval,       dse_eval_qual,     0, 0, 0,                       0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "EXIT",         dse_exit,       0,                 0, 0, 0,                       0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "FIND",         dse_f_blk,      dse_find_qual,     0, 0, cli_disallow_dse_find,   0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "HELP",         util_help,      0,                 0, 0, 0,                       0, VAL_DISALLOWED, 1, 0, 0, 0 },
{ "INTEGRIT",     dse_integ,      dse_integrit_qual, 0, 0, 0,                       0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "MAPS",         dse_maps,       dse_map_qual,      0, 0, cli_disallow_dse_maps,   0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "OPEN",         dse_open,       dse_open_qual,     0, 0, 0,                       0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "OVERWRITE",    dse_over,       dse_over_qual,     0, 0, 0,                       0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "PAGE",         dse_page,       0,                 0, 0, 0,                       0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "QUIT",         dse_exit,       0,                 0, 0, 0,                       0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "RANGE",        dse_range,      dse_range_qual,    0, 0, 0,                       0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "REMOVE",       dse_rmsb,       dse_remove_qual,   0, 0, cli_disallow_dse_remove, 0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "RESTORE",      dse_rest,       dse_restore_qual,  0, 0, 0,                       0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "SAVE",         dse_save,       dse_save_qual,     0, 0, cli_disallow_dse_save,   0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "SHIFT",        dse_shift,      dse_shift_qual,    0, 0, cli_disallow_dse_shift,  0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "SPAWN",        util_spawn,     0,                 0, 0, 0,                       0, VAL_DISALLOWED, 1, 0, 0, 0 },
{ "VERSION",      dse_version,    0,                 0, 0, 0,                       0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ "WCINIT",       dse_wcreinit,   0,                 0, 0, 0,                       0, VAL_DISALLOWED, 0, 0, 0, 0 },
{ 0 }
};

/* for SPAWN actually value is disallowed, but parameter is allowed. */
