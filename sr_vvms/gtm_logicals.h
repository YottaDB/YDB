/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_logicals.h - Logical names used by GT.M. */
/* within each group, the entries are in alpha order of the third column */
/* -------------------------- Common to Unix and VMS --------------------------  */

#define	GTM_DIST_LOG			"GTM$DIST"

/* Database */
#define	GTM_GBLDIR			"GTM$GBLDIR"
#define	GTM_BLKUPGRADE_FLAG		"GTM_BLKUPGRADE_FLAG"
#define	GTM_DBFILEXT_SYSLOG_DISABLE	"GTM_DBFILEXT_SYSLOG_DISABLE"
#define	GTM_ENV_XLATE			"GTM_ENV_TRANSLATE"
#define	GTM_FULLBLOCKWRITES		"GTM_FULLBLOCKWRITES"
#define	GTM_GDSCERT			"GTM_GDSCERT"
#define	GTM_GVDUPSETNOOP		"GTM_GVDUPSETNOOP"
#define	GTM_GVUNDEF_FATAL		"GTM_GVUNDEF_FATAL"
#define	GTM_TP_ALLOCATION_CLUE		"GTM_TP_ALLOCATION_CLUE"
#define	GTM_TPNOTACIDTIME		"GTM_TPNOTACIDTIME"
#define	GTM_TPRESTART_LOG_DELTA		"GTM_TPRESTART_LOG_DELTA"
#define	GTM_TPRESTART_LOG_LIMIT		"GTM_TPRESTART_LOG_FIRST"
#define	GTM_ZMAXTPTIME			"GTM_ZMAXTPTIME"

/* White-box testing */
#define	GTM_WHITE_BOX_TEST_CASE_COUNT	"GTM_WHITE_BOX_TEST_CASE_COUNT"
#define	GTM_WHITE_BOX_TEST_CASE_ENABLE	"GTM_WHITE_BOX_TEST_CASE_ENABLE"
#define	GTM_WHITE_BOX_TEST_CASE_NUMBER	"GTM_WHITE_BOX_TEST_CASE_NUMBER"

/* Indirection-cache */
#define GTM_MAX_INDRCACHE_COUNT         "GTM_MAX_INDRCACHE_COUNT"
#define GTM_MAX_INDRCACHE_MEMORY        "GTM_MAX_INDRCACHE_MEMORY"

/* MUPIP BACKUP */
# define GTM_BAK_TEMPDIR_LOG_NAME	"GTM_BAKTMPDIR"

/* Pattern match operator */
#define	PAT_FILE			"GTM_PATTERN_FILE"
#define	PAT_TABLE			"GTM_PATTERN_TABLE"

/* Alternative Collation */
#define	CT_PREFIX			"GTM_COLLATE_"
#define LCT_PREFIX			"GTM_LOCAL_COLLATE"
#define LCT_STDNULL			"GTM_LCT_STDNULL"

/* GTM processing versus M standard */
/* (see gtm_local_collate above) */
#define GTM_STDXKILL			"GTM_STDXKILL"

/* Miscellaneous */
#define	ZCOMPILE			"GTM$COMPILE"
#define	GTM_DEBUG_LEVEL_ENVLOG		"GTMDBGLVL"
#define	GTM_ZROUTINES			"GTM$ROUTINES"
#define GTM_BOOLEAN			"GTM_BOOLEAN"
#define	DISABLE_ALIGN_STRINGS		"GTM_DISABLE_ALIGNSTR"
#define GTM_MAX_SOCKETS			"GTM_MAX_SOCKETS"
#define GTM_MEMORY_RESERVE		"GTM_MEMORY_RESERVE"
#define	GTM_NOUNDEF			"GTM_NOUNDEF"
#define	GTM_PRINCIPAL			"GTM$PRINCIPAL"
#define	GTM_PROMPT			"GTM_PROMPT"
#define	GTM_SIDE_EFFECT			"GTM_SIDE_EFFECTS"
#define	SYSID				"GTM_SYSID"
#define GTM_MPROF_TESTING		"GTM_TRACE_GBL_NAME"
#define GTM_TRACE_GROUPS		"GTM_TRACE_GROUPS"
#define GTM_TRACE_TABLE_SIZE		"GTM_TRACE_TABLE_SIZE"
#define	ZDATE_FORM			"GTM_ZDATE_FORM"
#define	GTM_ZINTERRUPT			"GTM_ZINTERRUPT"
#define GTM_ZQUIT_ANYWAY		"GTM_ZQUIT_ANYWAY"
#define	ZTRAP_FORM			"GTM_ZTRAP_FORM"
#define	ZTRAP_NEW			"GTM_ZTRAP_NEW"
#define	ZYERROR				"GTM_ZYERROR"
#define GTM_MAX_STORALLOC		"GTM_MAX_STORALLOC"
/* -------------------------- VMS only --------------------------  */

/* Miscellaneous */
#define	GTM_MEMORY_NOACCESS_ADDR	"GTM_MEMORY_NOACCESS_ADDR"
#define	GTM_MEMORY_NOACCESS_COUNT	8				/* count of the above logicals which are parsed */

