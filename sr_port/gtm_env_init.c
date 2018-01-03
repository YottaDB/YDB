/****************************************************************
 *								*
 * Copyright (c) 2004-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"

#include <stddef.h>		/* For offsetof macro */

#include "gtm_logicals.h"
#include "gtm_multi_thread.h"
#include "logical_truth_value.h"
#include "trans_numeric.h"
#include "trans_log_name.h"
#include "gtmdbglvl.h"
#include "iosp.h"
#include "wbox_test_init.h"
#include "gtm_env_init.h"	/* for gtm_env_init() and gtm_env_init_sp() prototype */
#include "gt_timer.h"
#include "io.h"
#include "iosocketdef.h"
#include "gtm_malloc.h"
#include "cache.h"
#include "gdsroot.h"		/* needed for gdsfhead.h */
#include "gdskill.h"		/* needed for gdsfhead.h */
#include "gdsbt.h"		/* needed for gdsfhead.h */
#include "gdsfhead.h"		/* needed for MAXTOTALBLKS_MAX macro */
#include "mvalconv.h"
#include "fullbool.h"
#include "trace_table.h"
#include "parse_trctbl_groups.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdscc.h"
#include "filestruct.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "cli.h"

#ifdef DEBUG
#  define INITIAL_DEBUG_LEVEL GDL_Simple
#else
#  define INITIAL_DEBUG_LEVEL GDL_None
#endif

#ifdef FULLBLOCKWRITES
#  define DEFAULT_FBW_FLAG 1
#else
#  define DEFAULT_FBW_FLAG 0
#endif
#define SIZEOF_prombuf ggl_prombuf

/* gtm_dirtree_collhdr_always is only used in dbg code and hence doesn't need checking in the D9I10002703 subtest
 * Hence this env var is not defined in gtm_logicals.h as that is what the D9I10002703 subtest looks at for a list of env vars.
 */
#define	GTM_DIRTREE_COLLHDR_ALWAYS	"$gtm_dirtree_collhdr_always"

GBLREF	boolean_t	dollar_zquit_anyway;	/* if TRUE compile QUITs to not care whether or not they're from an extrinsic */
GBLREF	uint4		gtmDebugLevel; 		/* Debug level (0 = using default sm module so with
						   a DEBUG build, even level 0 implies basic debugging) */
GBLREF	int4		gtm_fullblockwrites;	/* Do full (not partial) database block writes */
GBLREF	boolean_t	certify_all_blocks;
GBLREF	uint4		gtm_blkupgrade_flag;	/* controls whether dynamic block upgrade is attempted or not */
GBLREF	boolean_t	gtm_dbfilext_syslog_disable;	/* control whether db file extension message is logged or not */
GBLREF	uint4		gtm_max_sockets;	/* Maximum sockets in a socket device that can be created by this process */
GBLREF	bool		undef_inhibit;
GBLREF	uint4		outOfMemoryMitigateSize;	/* Reserve that we will freed to help cleanup if run out of memory */
GBLREF	uint4		max_cache_memsize;	/* Maximum bytes used for indirect cache object code */
GBLREF	uint4		max_cache_entries;	/* Maximum number of cached indirect compilations */
GBLREF	block_id	gtm_tp_allocation_clue;	/* block# hint to start allocation for created blocks in TP */
GBLREF	boolean_t	gtm_stdxkill;		/* Use M Standard exclusive kill instead of historical GTM */
GBLREF	boolean_t	ztrap_new;		/* Each time $ZTRAP is set it is automatically NEW'd */
GBLREF	size_t		gtm_max_storalloc;	/* Used for testing: creates an allocation barrier */

void	gtm_env_init(void)
{
	boolean_t		ret, is_defined;
	char			buf[MAX_TRANS_NAME_LEN];
	double			time;
	int			status2, i, j;
	int4			status;
	mstr			val, trans;
	uint4			tdbglvl, tmsock, reservesize, memsize, cachent, trctblsize, trctblbytes;
	uint4			max_threads, max_procs;
	int4			temp_gtm_strpllim;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!TREF(gtm_env_init_started))
	{
		TREF(gtm_env_init_started) = TRUE;
		/* See if a debug level has been specified. Do this first since gtmDebugLevel needs
		 * to be initialized before any mallocs are done in the system.
		 */
		gtmDebugLevel = INITIAL_DEBUG_LEVEL;
		val.addr = GTM_DEBUG_LEVEL_ENVLOG;
		val.len = SIZEOF(GTM_DEBUG_LEVEL_ENVLOG) - 1;
		if (tdbglvl = trans_numeric(&val, &is_defined, TRUE)) /* Note assignment!! */
		{	/* Some kind of debugging was asked for.. */
			tdbglvl |= GDL_Simple;			/* Make sure simple debugging turned on if any is */
			if ((GDL_SmChkFreeBackfill | GDL_SmChkAllocBackfill) & tdbglvl)
				tdbglvl |= GDL_SmBackfill;	/* Can't check it unless it's filled in */
			if (GDL_SmStorHog & tdbglvl)
				tdbglvl |= GDL_SmBackfill | GDL_SmChkAllocBackfill;
			gtmDebugLevel |= tdbglvl;
		}
		/* gtm_boolean environment/logical */
		val.addr = GTM_BOOLEAN;
		val.len = SIZEOF(GTM_BOOLEAN) - 1;
		TREF(gtm_fullbool) = trans_numeric(&val, &is_defined, TRUE);
		switch (TREF(gtm_fullbool))
		{
			case GTM_BOOL:			/* original GT.M short-circuit Boolean evaluation with naked maintenance */
			case FULL_BOOL:			/* standard behavior - evaluate everything with a side effect */
			case FULL_BOOL_WARN:		/* like FULL_BOOL but give compiler warnings when it makes a difference */
				break;
			default:
				TREF(gtm_fullbool) = GTM_BOOL;
		}
		/* gtm_side_effects environment/logical */
		val.addr = GTM_SIDE_EFFECT;
		val.len = SIZEOF(GTM_SIDE_EFFECT) - 1;
		TREF(side_effect_handling) = trans_numeric(&val, &is_defined, TRUE);
		switch (TREF(side_effect_handling))
		{
			case OLD_SE:				/* ignore side effect implications */
			case STD_SE:				/* reorder argument processing for left-to-right side effects */
			case SE_WARN:				/* like STD but give compiler warnings when it makes a difference */
				break;
			default:
				TREF(side_effect_handling) = OLD_SE;	/* default is: ignore side effect implications */
		}
		if ((OLD_SE != TREF(side_effect_handling)) && (GTM_BOOL == TREF(gtm_fullbool)))	/* side effect implies full bool */
			TREF(gtm_fullbool) = FULL_BOOL;
		/* NOUNDEF environment/logical */
		val.addr = GTM_NOUNDEF;
		val.len = SIZEOF(GTM_NOUNDEF) - 1;
		assert(FALSE == undef_inhibit);	/* should have been set to FALSE at global variable definition time */
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			undef_inhibit = ret; /* if the logical is not defined, we want undef_inhibit to take its default value */
		/* gtm_trace_gbl_name environment; it controls implicit MPROF testing */
		val.addr = GTM_MPROF_TESTING;
		val.len = SIZEOF(GTM_MPROF_TESTING) - 1;
		if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long)))
		{	/* Note assignment above */
			if (SIZEOF(buf) >= trans.len)
			{
				if (('0' == (char)(*trans.addr)) || (0 == trans.len))
				{
					(TREF(mprof_env_gbl_name)).str.len = 0;
					/* this malloc is just so that mprof_env_gbl_name.str.addr is not NULL for subsequent
					 * checks in gtm_startup.c and gtm$startup.c
					 */
					(TREF(mprof_env_gbl_name)).str.addr = malloc(1);
				} else
				{
					(TREF(mprof_env_gbl_name)).str.len = trans.len;
					(TREF(mprof_env_gbl_name)).str.addr = malloc(trans.len);
					memcpy((TREF(mprof_env_gbl_name)).str.addr, trans.addr, trans.len);
				}
				(TREF(mprof_env_gbl_name)).mvtype = MV_STR;
			}
		} else
			(TREF(mprof_env_gbl_name)).str.addr = NULL;
		val.addr = GTM_POOLLIMIT;
		val.len = SIZEOF(GTM_POOLLIMIT) - 1;
		if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long)))
		{	/* Note assignment above */
			if (SIZEOF(buf) >= trans.len)
			{
				if (('0' == (char)(*trans.addr)) || (0 == trans.len))
					(TREF(gbuff_limit)).str.len = 0;
				else
				{
					(TREF(gbuff_limit)).str.len = trans.len;
					(TREF(gbuff_limit)).str.addr = malloc(trans.len);
					memcpy((TREF(gbuff_limit)).str.addr, trans.addr, trans.len);
				}
			}
		} else
			(TREF(gbuff_limit)).str.len = 0;
		(TREF(gbuff_limit)).mvtype = MV_STR;
#		ifdef DEBUG
		/* GTM_GVUNDEF_FATAL environment/logical */
		val.addr = GTM_GVUNDEF_FATAL;
		val.len = SIZEOF(GTM_GVUNDEF_FATAL) - 1;
		assert(FALSE == TREF(gtm_gvundef_fatal));	/* should have been set to FALSE by gtm_threadgbl_defs */
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			TREF(gtm_gvundef_fatal) = ret; /* if logical is not defined, gtm_gvundef_fatal takes the default value */
		/* GTM_DIRTREE_COLLHDR_ALWAYS environment/logical */
		val.addr = GTM_DIRTREE_COLLHDR_ALWAYS;
		val.len = SIZEOF(GTM_DIRTREE_COLLHDR_ALWAYS) - 1;
		assert(FALSE == TREF(gtm_dirtree_collhdr_always));	/* should have been set to FALSE by gtm_threadgbl_defs */
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			TREF(gtm_dirtree_collhdr_always) = ret; /* if logical is not defined, the TREF takes the default value */
#		endif
		/* Initialize variable that controls TP allocation clue (for created blocks) */
		val.addr = GTM_TP_ALLOCATION_CLUE;
		val.len = SIZEOF(GTM_TP_ALLOCATION_CLUE) - 1;
		gtm_tp_allocation_clue = (block_id)trans_numeric(&val, &is_defined, TRUE);
		if (!is_defined)
			gtm_tp_allocation_clue = (block_id)MAXTOTALBLKS_MAX;
		/* Full Database-block Write mode */
		val.addr = GTM_FULLBLOCKWRITES;
		val.len = SIZEOF(GTM_FULLBLOCKWRITES) - 1;
		gtm_fullblockwrites = (int)logical_truth_value(&val, FALSE, &is_defined);
		if (!is_defined) /* Variable not defined */
			gtm_fullblockwrites = DEFAULT_FBW_FLAG;
		else if (!gtm_fullblockwrites) /* Set to false */
			gtm_fullblockwrites = FALSE;
		else /* Set to true, exam for FULL_DATABASE_WRITE */
		{
			gtm_fullblockwrites = trans_numeric(&val, &is_defined, TRUE);
			switch(gtm_fullblockwrites)
			{
			case FULL_FILESYSTEM_WRITE:
			case FULL_DATABASE_WRITE:
				/* Both these values are valid */
				break;
			default:
				/* Else, assume FULL_FILESYSTEM_WRITE */
				gtm_fullblockwrites = FULL_FILESYSTEM_WRITE;
				break;
			}
		}
		/* GDS Block certification */
		val.addr = GTM_GDSCERT;
		val.len = SIZEOF(GTM_GDSCERT) - 1;
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			certify_all_blocks = ret; /* if the logical is not defined, we want to take default value */
		/* Initialize null subscript's collation order */
		val.addr = LCT_STDNULL;
		val.len = SIZEOF(LCT_STDNULL) - 1;
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			TREF(local_collseq_stdnull) = ret;
		/* Initialize eXclusive Kill variety (GTM vs M Standard) */
		val.addr = GTM_STDXKILL;
		val.len = SIZEOF(GTM_STDXKILL) - 1;
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			gtm_stdxkill = ret;
		/* Initialize variables for white box testing. Even though these white-box test variables only control the
		 * flow of the DBG builds, the PRO builds check on these variables (for example, in tp_restart.c to decide
		 * whether to fork_n_core or not) so need to do this initialization for PRO builds as well.
		 */
		wbox_test_init();
		/* Initialize variable that controls dynamic GT.M block upgrade */
		val.addr = GTM_BLKUPGRADE_FLAG;
		val.len = SIZEOF(GTM_BLKUPGRADE_FLAG) - 1;
		gtm_blkupgrade_flag = trans_numeric(&val, &is_defined, TRUE);
		/* Initialize whether database file extensions need to be logged in the operator log */
		val.addr = GTM_DBFILEXT_SYSLOG_DISABLE;
		val.len = SIZEOF(GTM_DBFILEXT_SYSLOG_DISABLE) - 1;
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			gtm_dbfilext_syslog_disable = ret; /* if the logical is not defined, we want to take default value */
		/* Initialize maximum sockets in a single socket device createable by this process */
		gtm_max_sockets = MAX_N_SOCKET;
		val.addr = GTM_MAX_SOCKETS;
		val.len = SIZEOF(GTM_MAX_SOCKETS) - 1;
		if ((tmsock = trans_numeric(&val, &is_defined, TRUE)) && MAX_MAX_N_SOCKET > tmsock) /* Note assignment!! */
			gtm_max_sockets = tmsock;
		/* Initialize storage to allocate and keep in our back pocket in case run out of memory */
		outOfMemoryMitigateSize = GTM_MEMORY_RESERVE_DEFAULT;
		val.addr = GTM_MEMORY_RESERVE;
		val.len = SIZEOF(GTM_MEMORY_RESERVE) - 1;
		if (reservesize = trans_numeric(&val, &is_defined, TRUE)) /* Note assignment!! */
			outOfMemoryMitigateSize = reservesize;
		/* Initialize indirect cache limits (max memory, max entries) */
		max_cache_memsize = MAX_CACHE_MEMSIZE * 1024;
		val.addr = GTM_MAX_INDRCACHE_MEMORY;
		val.len = SIZEOF(GTM_MAX_INDRCACHE_MEMORY) - 1;
		if (memsize = trans_numeric(&val, &is_defined, TRUE)) /* Note assignment!! */
			max_cache_memsize = memsize * 1024;
		max_cache_entries = MAX_CACHE_ENTRIES;
		val.addr = GTM_MAX_INDRCACHE_COUNT;
		val.len = SIZEOF(GTM_MAX_INDRCACHE_COUNT) - 1;
		if (cachent = trans_numeric(&val, &is_defined, TRUE)) /* Note assignment!! */
			max_cache_entries = cachent;
		/* Initialize ZQUIT to control funky QUIT compilation */
		val.addr = GTM_ZQUIT_ANYWAY;
		val.len = SIZEOF(GTM_ZQUIT_ANYWAY) - 1;
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			dollar_zquit_anyway = ret;
		/* Initialize ZPROMPT to desired GTM prompt or default */
		val.addr = GTM_PROMPT;
		val.len = SIZEOF(GTM_PROMPT) - 1;
		if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long)))
		{	/* Non-standard prompt requested */
			assert(SIZEOF(buf) > trans.len);
			if (SIZEOF_prombuf >= trans.len)
			{
				(TREF(gtmprompt)).len = trans.len;
				memcpy((TREF(gtmprompt)).addr, trans.addr, trans.len);
			}
		}
		/* Initialize tpnotacidtime */
		(TREF(tpnotacidtime)).m[1] = TPNOTACID_DEFAULT_TIME;
		val.addr = GTM_TPNOTACIDTIME;
		val.len = SIZEOF(GTM_TPNOTACIDTIME) - 1;
		if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long)))
		{
			assert(SIZEOF(buf) > trans.len);
			*(char *)(buf + trans.len) = 0;
			errno = 0;
			time = strtod(buf, NULL);
			if ((ERANGE != errno) && (TPNOTACID_MAX_TIME >= time))
				(TREF(tpnotacidtime)).m[1] = time * MILLISECS_IN_SEC;
		}	/* gtm_startup completes initialization of the tpnotacidtime mval */
		/* Initialize $gtm_tprestart_log_first */
		val.addr = GTM_TPRESTART_LOG_FIRST;
		val.len = STR_LIT_LEN(GTM_TPRESTART_LOG_FIRST);
		TREF(tprestart_syslog_first) = trans_numeric(&val, &is_defined, TRUE);
		if (0 > TREF(tprestart_syslog_first))
			TREF(tprestart_syslog_first) = 0;
		/* Initialize $gtm_tprestart_log_delta */
		val.addr = GTM_TPRESTART_LOG_DELTA;
		val.len = STR_LIT_LEN(GTM_TPRESTART_LOG_DELTA);
		TREF(tprestart_syslog_delta) = trans_numeric(&val, &is_defined, TRUE);
		if (0 > TREF(tprestart_syslog_delta))
			TREF(tprestart_syslog_delta) = 0;
		/* Initialize $gtm_nontprestart_log_first */
		val.addr = GTM_NONTPRESTART_LOG_FIRST;
		val.len = STR_LIT_LEN(GTM_NONTPRESTART_LOG_FIRST);
		TREF(nontprestart_log_first) = trans_numeric(&val, &is_defined, TRUE);
		if (0 > TREF(nontprestart_log_first))
			TREF(nontprestart_log_first) = 0;
		/* Initialize $gtm_nontprestart_log_delta */
		val.addr = GTM_NONTPRESTART_LOG_DELTA;
		val.len = STR_LIT_LEN(GTM_NONTPRESTART_LOG_DELTA);
		TREF(nontprestart_log_delta) = trans_numeric(&val, &is_defined, TRUE);
		if (0 > TREF(nontprestart_log_delta))
			TREF(nontprestart_log_delta) = 0;
		/* See if this is a GT.M Development environment, not a production environment */
		if (GETENV("gtm_environment_init"))
			TREF(gtm_environment_init) = TRUE; /* in-house */
		/* See if a trace table is desired. If we have been asked to trace one or more groups, we also
		 * see if a specific size has been specified. A default size is provided.
		 */
		val.addr = GTM_TRACE_GROUPS;
		val.len = SIZEOF(GTM_TRACE_GROUPS) - 1;
		if (SS_NORMAL == (status = TRANS_LOG_NAME(&val, &trans, buf, SIZEOF(buf), do_sendmsg_on_log2long)))
		{	/* Trace-group(s) have been declared - figure out which ones */
			assert(SIZEOF(buf) > trans.len);
			parse_trctbl_groups(&trans);
			if (0 != TREF(gtm_trctbl_groups))
			{	/* At least one valid group was specified */
				val.addr = GTM_TRACE_TABLE_SIZE;
				val.len = SIZEOF(GTM_TRACE_TABLE_SIZE) - 1;
				trctblsize = trans_numeric(&val, &is_defined, TRUE);
				if (0 < (trctblsize = (0 < trctblsize) ? trctblsize : TRACE_TABLE_SIZE_DEFAULT)) /* assignment! */
				{
					trctblbytes = trctblsize * SIZEOF(trctbl_entry);
					TREF(gtm_trctbl_start) = malloc(trctblbytes);
					TREF(gtm_trctbl_end) = TREF(gtm_trctbl_start) + trctblsize;
					TREF(gtm_trctbl_cur) = TREF(gtm_trctbl_start) - 1; /* So doesn't skip 1st entry */
					memset(TREF(gtm_trctbl_start), 0, trctblbytes);
				}
			}
		}
#		ifdef	UNIX
		/* Initialize jnl_extract_nocol */
		val.addr = GTM_EXTRACT_NOCOL;
		val.len = STR_LIT_LEN(GTM_EXTRACT_NOCOL);
		TREF(jnl_extract_nocol) = trans_numeric(&val, &is_defined, TRUE);
#		endif
		/* Initialize dollar_zmaxtptime */
		val.addr = GTM_ZMAXTPTIME;
		val.len = SIZEOF(GTM_ZMAXTPTIME) - 1;
		if ((status = trans_numeric(&val, &is_defined, TRUE)) && (0 <= status) && (TPTIMEOUT_MAX_TIME >= status))
			TREF(dollar_zmaxtptime) = status;	 /* NOTE assignment above */
		/* See if $gtm_ztrap_new/GTM_ZTRAP_NEW has been specified */
		val.addr = ZTRAP_NEW;
		val.len = SIZEOF(ZTRAP_NEW) - 1;
		ztrap_new = logical_truth_value(&val, FALSE, NULL);
		/* See if $gtm_max_storalloc is set */
		val.addr = GTM_MAX_STORALLOC;
		val.len = SIZEOF(GTM_MAX_STORALLOC) - 1;
		gtm_max_storalloc = trans_numeric(&val, &is_defined, TRUE);
		/* See if $gtm_mupjnl_parallel is set */
		val.addr = GTM_MUPJNL_PARALLEL;
		val.len = SIZEOF(GTM_MUPJNL_PARALLEL) - 1;
		gtm_mupjnl_parallel = trans_numeric(&val, &is_defined, TRUE);
		if (!is_defined)
			gtm_mupjnl_parallel = 1;
		/* See if $gtm_string_pool_limit is set */
		val.addr = GTM_STRPLLIM;
		val.len = SIZEOF(GTM_STRPLLIM) - 1;
		temp_gtm_strpllim = trans_numeric(&val, &is_defined, TRUE);
		if (0 < temp_gtm_strpllim)
			TREF(gtm_strpllim) = temp_gtm_strpllim;
		/* Platform specific initializations */
		gtm_env_init_sp();
	}
}
