/****************************************************************
 *								*
 * Copyright (c) 2004-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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

#include "ydb_logicals.h"
#include "gtm_multi_thread.h"
#include "ydb_logical_truth_value.h"
#include "ydb_trans_numeric.h"
#include "ydb_trans_log_name.h"
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
#include "tp.h"
#include "cli.h"
#include "repl_filter.h"
#include "gds_blk_upgrade.h"
#include "mlkdef.h"
#include "getstorage.h"

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
#define SIZEOF_prombuf		ggl_prombuf
#define SIZEOF_ydbmsgprefixbuf	ggl_ydbmsgprefixbuf

GBLREF	boolean_t	dollar_zquit_anyway;	/* if TRUE compile QUITs to not care whether or not they're from an extrinsic */
GBLREF	uint4		ydbDebugLevel; 		/* Debug level (0 = using default sm module so with
						   a DEBUG build, even level 0 implies basic debugging) */
GBLREF	boolean_t	ydbSystemMalloc;	/* Use the system's malloc() instead of our own */
GBLREF	boolean_t	certify_all_blocks;
GBLREF	uint4		ydb_blkupgrade_flag;	/* controls whether dynamic block upgrade is attempted or not */
GBLREF	boolean_t	ydb_dbfilext_syslog_disable;	/* control whether db file extension message is logged or not */
GBLREF	uint4		ydb_max_sockets;	/* Maximum sockets in a socket device that can be created by this process */
GBLREF	bool		undef_inhibit;
GBLREF	uint4		outOfMemoryMitigateSize;	/* Reserve that we will freed to help cleanup if run out of memory */
GBLREF	uint4		max_cache_memsize;	/* Maximum bytes used for indirect cache object code */
GBLREF	uint4		max_cache_entries;	/* Maximum number of cached indirect compilations */
GBLREF	boolean_t	ydb_stdxkill;		/* Use M Standard exclusive kill instead of historical GTM */
GBLREF	boolean_t	ztrap_new;		/* Each time $ZTRAP is set it is automatically NEW'd */
GBLREF	size_t		ydb_max_storalloc;	/* Used for testing: creates an allocation barrier */
GBLREF	int		ydb_repl_filter_timeout;/* # of seconds that source server waits before issuing FILTERTIMEDOUT */
GBLREF  boolean_t 	dollar_test_default; 	/* Default value taken by dollar_truth via dollar_test_default */
GBLREF	boolean_t	gtm_nofflf;		/* Used to control "write #" behavior ref GTM-9136 */
GBLREF	size_t		zmalloclim;		/* ISV memory warning of MALLOCCRIT in bytes */
GBLREF	boolean_t	malloccrit_issued;	/* MEMORY error limit set at time of MALLOCCRIT */
GBLREF	bool		pin_shared_memory;	/* pin shared memory into physical memory on creation */
GBLREF	bool		hugetlb_shm_enabled;	/* allocate shared memory backed by huge pages */

#ifdef DEBUG
GBLREF	block_id	ydb_skip_bml_num;
#endif

void	gtm_env_init(void)
{
	boolean_t		ret, is_defined;
	char			buf[MAX_TRANS_NAME_LEN];
	double			time;
	int			status2, i, j;
	int4			status;
	mstr			trans;
	size_t			tmp_malloc_limit;
	uint4			tdbglvl, tmsock, reservesize, memsize, cachent, trctblsize, trctblbytes;
	uint4			max_threads, max_procs;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!TREF(gtm_env_init_started))
	{
		TREF(gtm_env_init_started) = TRUE;
		/* See if ydb_dbglvl has been specified. Do this first since ydbDebugLevel needs
		 * to be initialized before any mallocs are done in the system.
		 */
		ydbDebugLevel = INITIAL_DEBUG_LEVEL;
		tdbglvl = ydb_trans_numeric(YDBENVINDX_DBGLVL, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		if (is_defined)
		{	/* Some kind of debugging was asked for.. */
			tdbglvl |= GDL_Simple;			/* Make sure simple debugging turned on if any is */
			if ((GDL_SmChkFreeBackfill | GDL_SmChkAllocBackfill) & tdbglvl)
				tdbglvl |= GDL_SmBackfill;	/* Can't check it unless it's filled in */
			if (GDL_SmStorHog & tdbglvl)
				tdbglvl |= GDL_SmBackfill | GDL_SmChkAllocBackfill;
			ydbDebugLevel |= tdbglvl;
			ydbSystemMalloc = (GDL_UseSystemMalloc & ydbDebugLevel);
			if (ydbSystemMalloc)
				ydbDebugLevel &= !GDL_SmAllMallocDebug;
		}
<<<<<<< HEAD
		/* See if ydb_msgprefix is specified. If so store it in TREF(ydbmsgprefix).
		 * Note: Default value is already stored in "gtm_threadgbl_init".
		 * Do this initialization before most other variables so any error messages later issued in this module
		 * have the correct msgprefix.
		 */
		if (SS_NORMAL ==
			(status = ydb_trans_log_name(YDBENVINDX_MSGPREFIX, &trans, buf, SIZEOF(buf), IGNORE_ERRORS_TRUE, NULL)))
=======
		/* gtm_pinshm environment/logical */
		val.addr = GTM_PINSHM;
		val.len = SIZEOF(GTM_PINSHM) - 1;
		pin_shared_memory = logical_truth_value(&val, FALSE, &is_defined);
		/* gtm_hugepages environment/logical */
		val.addr = GTM_HUGETLB_SHM;
		val.len = SIZEOF(GTM_HUGETLB_SHM) - 1;
		hugetlb_shm_enabled = logical_truth_value(&val, FALSE, &is_defined);
		/* gtm_boolean environment/logical */
		val.addr = GTM_BOOLEAN;
		val.len = SIZEOF(GTM_BOOLEAN) - 1;
		TREF(gtm_fullbool) = trans_numeric(&val, &is_defined, TRUE);
		switch (TREF(gtm_fullbool))
>>>>>>> 732d6f04 (GT.M V7.0-005)
		{
			assert(SIZEOF(buf) > trans.len);
			if (SIZEOF_ydbmsgprefixbuf > trans.len)
			{
				(TREF(ydbmsgprefix)).len = trans.len;
				memcpy((TREF(ydbmsgprefix)).addr, trans.addr, trans.len);
				(TREF(ydbmsgprefix)).addr[trans.len] = '\0';	/* need null terminated "fac" in "gtm_getmsg" */
			}
		}
		/* ydb_boolean environment/logical */
		TREF(ydb_fullbool) = ydb_trans_numeric(YDBENVINDX_BOOLEAN, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		switch (TREF(ydb_fullbool))
		{
			case YDB_BOOL:			/* original GT.M short-circuit Boolean evaluation with naked maintenance */
			case FULL_BOOL:			/* standard behavior - evaluate everything with a side effect */
			case FULL_BOOL_WARN:		/* like FULL_BOOL but give compiler warnings when it makes a difference */
				break;
			default:
				TREF(ydb_fullbool) = YDB_BOOL;
		}
		/* ydb_side_effects environment/logical */
		TREF(side_effect_handling) = ydb_trans_numeric(YDBENVINDX_SIDE_EFFECTS, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		switch (TREF(side_effect_handling))
		{
			case OLD_SE:				/* ignore side effect implications */
			case STD_SE:				/* reorder argument processing for left-to-right side effects */
			case SE_WARN:				/* like STD but give compiler warnings when it makes a difference */
				break;
			default:
				TREF(side_effect_handling) = OLD_SE;	/* default is: ignore side effect implications */
		}
		if ((OLD_SE != TREF(side_effect_handling)) && (YDB_BOOL == TREF(ydb_fullbool)))	/* side effect implies full bool */
			TREF(ydb_fullbool) = FULL_BOOL;
		/* NOUNDEF environment/logical */
		assert(FALSE == undef_inhibit);	/* should have been set to FALSE at global variable definition time */
		ret = ydb_logical_truth_value(YDBENVINDX_NOUNDEF, FALSE, &is_defined);
		if (is_defined)
			undef_inhibit = ret; /* if the logical is not defined, we want undef_inhibit to take its default value */
		/* ydb_trace_gbl_name environment variable controls implicit MPROF testing */
		if (SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_TRACE_GBL_NAME, &trans, buf, SIZEOF(buf),
												IGNORE_ERRORS_TRUE, NULL)))
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
		if (SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_POOLLIMIT, &trans, buf, SIZEOF(buf),
											IGNORE_ERRORS_TRUE, NULL)))
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
		/* ydb_gvundef_fatal environment/logical */
		assert(FALSE == TREF(ydb_gvundef_fatal));	/* should have been set to FALSE by gtm_threadgbl_defs */
		ret = ydb_logical_truth_value(YDBENVINDX_GVUNDEF_FATAL, FALSE, &is_defined);
		if (is_defined)
			TREF(ydb_gvundef_fatal) = ret; /* if logical is not defined, ydb_gvundef_fatal takes the default value */
		/* ydb_lockhash_n_bits environment/logical */
		TREF(ydb_lockhash_n_bits) = ydb_trans_numeric(YDBENVINDX_LOCKHASH_N_BITS, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		if (!is_defined)
			TREF(ydb_lockhash_n_bits) = 0;	/* Do not tamper with hash value in dbg */
		else if ((SIZEOF(mlk_subhash_val_t) * BITS_PER_UCHAR) < TREF(ydb_lockhash_n_bits))
			TREF(ydb_lockhash_n_bits) = SIZEOF(mlk_subhash_val_t) * BITS_PER_UCHAR;	/* Keep all bits of hash val */
		/* else Keep only at most N bits of hash value in dbg where TREF(ydb_lockhash_n_bits) == N */
		/* ydb_dirtree_collhdr_always environment/logical */
		assert(FALSE == TREF(ydb_dirtree_collhdr_always));	/* should have been set to FALSE by gtm_threadgbl_defs */
		ret = ydb_logical_truth_value(YDBENVINDX_DIRTREE_COLLHDR_ALWAYS, FALSE, &is_defined);
		if (is_defined)
			TREF(ydb_dirtree_collhdr_always) = ret; /* if logical is not defined, the TREF takes the default value */
		/* ydb_test_4g_db_blks env var. If set to a non-zero value of N, it implies blocks from the 1st local bitmap
		 * to the N-1th local bitmap block are not allocated in the db file. After the 0th local bitmap block, the
		 * Nth local bitmap block is where block allocation continues. This helps test records in the database blocks
		 * that contain 8-byte block numbers where the higher order 4-byte is non-zero. This effectively tests all
		 * code (mostly added in GT.M V7.0-000 and merged into YottaDB r2.00) that handles 8-byte block numbers.
		 * A 0 value implies this scheme is disabled.
		 */
		ydb_skip_bml_num = (block_id)ydb_trans_numeric(YDBENVINDX_TEST_4G_DB_BLKS, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		if (!is_defined)
			ydb_skip_bml_num = 0;
		else
			ydb_skip_bml_num *= BLKS_PER_LMAP;
#		endif
		/* GDS Block certification */
		ret = ydb_logical_truth_value(YDBENVINDX_GDSCERT, FALSE, &is_defined);
		if (is_defined)
			certify_all_blocks = ret; /* if the logical is not defined, we want to take default value */
		/* Initialize null subscript's collation order */
		ret = ydb_logical_truth_value(YDBENVINDX_LCT_STDNULL, FALSE, &is_defined);
		if (is_defined)
			TREF(local_collseq_stdnull) = ret;
		else
			TREF(local_collseq_stdnull) = TRUE;
		/* Initialize eXclusive Kill variety (GTM vs M Standard) */
		ret = ydb_logical_truth_value(YDBENVINDX_STDXKILL, FALSE, &is_defined);
		if (is_defined)
			ydb_stdxkill = ret;
		/* Initialize variables for white box testing. Even though these white-box test variables only control the
		 * flow of the DBG builds, the PRO builds check on these variables (for example, in tp_restart.c to decide
		 * whether to fork_n_core or not) so need to do this initialization for PRO builds as well.
		 */
		wbox_test_init();
		/* Initialize variable that controls dynamic block upgrade */
		ydb_blkupgrade_flag = ydb_trans_numeric(YDBENVINDX_BLKUPGRADE_FLAG, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		/* If ydb_blkupgrade_flag is outside of valid range of choices, set it back to default value */
		if ((UPGRADE_NEVER != ydb_blkupgrade_flag) && (UPGRADE_ALWAYS != ydb_blkupgrade_flag))
			ydb_blkupgrade_flag = UPGRADE_IF_NEEDED;
		/* Initialize whether database file extensions need to be logged in the operator log */
		ret = ydb_logical_truth_value(YDBENVINDX_DBFILEXT_SYSLOG_DISABLE, FALSE, &is_defined);
		if (is_defined)
			ydb_dbfilext_syslog_disable = ret; /* if the logical is not defined, we want to take default value */
		/* Initialize maximum sockets in a single socket device createable by this process */
		ydb_max_sockets = MAX_N_SOCKET;
		if ((tmsock = ydb_trans_numeric(YDBENVINDX_MAX_SOCKETS, &is_defined, IGNORE_ERRORS_TRUE, NULL))
							&& MAX_MAX_N_SOCKET > tmsock) /* Note assignment!! */
			ydb_max_sockets = tmsock;
		/* Initialize TCP_KEEPIDLE and by implication SO_KEEPALIVE */
		TREF(ydb_socket_keepalive_idle) = ydb_trans_numeric(YDBENVINDX_SOCKET_KEEPALIVE_IDLE, &is_defined,
											IGNORE_ERRORS_TRUE, NULL);
		if (0 > TREF(ydb_socket_keepalive_idle))
			TREF(ydb_socket_keepalive_idle) = 0;
		/* Initialize storage to allocate and keep in our back pocket in case run out of memory */
		outOfMemoryMitigateSize = GTM_MEMORY_RESERVE_DEFAULT;
		reservesize = ydb_trans_numeric(YDBENVINDX_MEMORY_RESERVE, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		if (reservesize)
			outOfMemoryMitigateSize = reservesize;
		/* Initialize indirect cache limits (max memory, max entries) */
		max_cache_memsize = DEFAULT_INDRCACHE_KBSIZE * BIN_ONE_K;
		memsize = ydb_trans_numeric(YDBENVINDX_MAX_INDRCACHE_MEMORY, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		if (memsize)
			max_cache_memsize = ((MAX_INDRCACHE_KBSIZE > memsize) ? memsize : MAX_INDRCACHE_KBSIZE) * BIN_ONE_K;
		max_cache_entries = DEFAULT_INRDCACHE_ENTRIES;
		cachent = ydb_trans_numeric(YDBENVINDX_MAX_INDRCACHE_COUNT, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		if (cachent)
			max_cache_entries = cachent;
		/* Initialize ZQUIT to control funky QUIT compilation */
		ret = ydb_logical_truth_value(YDBENVINDX_ZQUIT_ANYWAY, FALSE, &is_defined);
		if (is_defined)
			dollar_zquit_anyway = ret;
		/* Initialize ZPROMPT to desired GTM prompt or default */
		if (SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_PROMPT, &trans, buf, SIZEOF(buf),
											IGNORE_ERRORS_TRUE, NULL)))
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
		if (SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_TPNOTACIDTIME, &trans, buf, SIZEOF(buf),
												IGNORE_ERRORS_TRUE, NULL)))
		{
			assert(SIZEOF(buf) > trans.len);
			assert('\0' == buf[trans.len]);
			errno = 0;
			time = strtod(buf, NULL);
			if ((ERANGE != errno) && (TPNOTACID_MAX_TIME >= time))
				(TREF(tpnotacidtime)).m[1] = time * MILLISECS_IN_SEC;
		}	/* gtm_startup completes initialization of the tpnotacidtime mval */
		/* Initialize $ydb_tprestart_log_first */
		TREF(tprestart_syslog_first) = ydb_trans_numeric(YDBENVINDX_TPRESTART_LOG_FIRST, &is_defined,
												IGNORE_ERRORS_TRUE, NULL);
		if (0 > TREF(tprestart_syslog_first))
			TREF(tprestart_syslog_first) = 0;
		/* Initialize $ydb_tprestart_log_delta */
		TREF(tprestart_syslog_delta) = ydb_trans_numeric(YDBENVINDX_TPRESTART_LOG_DELTA, &is_defined,
												IGNORE_ERRORS_TRUE, NULL);
		if (0 > TREF(tprestart_syslog_delta))
			TREF(tprestart_syslog_delta) = 0;
		/* Initialize $ydb_nontprestart_log_first */
		TREF(nontprestart_log_first) = ydb_trans_numeric(YDBENVINDX_NONTPRESTART_LOG_FIRST, &is_defined,
												IGNORE_ERRORS_TRUE, NULL);
		if (0 > TREF(nontprestart_log_first))
			TREF(nontprestart_log_first) = 0;
		/* Initialize $ydb_nontprestart_log_delta */
		TREF(nontprestart_log_delta) = ydb_trans_numeric(YDBENVINDX_NONTPRESTART_LOG_DELTA, &is_defined,
												IGNORE_ERRORS_TRUE, NULL);
		if (0 > TREF(nontprestart_log_delta))
			TREF(nontprestart_log_delta) = 0;
		/* See if this is a GT.M Development environment, not a production environment */
		ret = ydb_logical_truth_value(YDBENVINDX_ENVIRONMENT_INIT, FALSE, &is_defined);
		if (is_defined)
			TREF(ydb_environment_init) = TRUE; /* in-house */
#		ifdef DEBUG
		/* See if a trace table is desired. If we have been asked to trace one or more groups, we also
		 * see if a specific size has been specified. A default size is provided.
		 */
		if (SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_TRACE_GROUPS, &trans, buf, SIZEOF(buf),
												IGNORE_ERRORS_TRUE, NULL)))
		{	/* Trace-group(s) have been declared - figure out which ones */
			assert(SIZEOF(buf) > trans.len);
			parse_trctbl_groups(&trans);
			if (0 != TREF(gtm_trctbl_groups))
			{	/* At least one valid group was specified */
				trctblsize = ydb_trans_numeric(YDBENVINDX_TRACE_TABLE_SIZE, &is_defined, IGNORE_ERRORS_TRUE, NULL);
				if (0 < (trctblsize = (0 < trctblsize) ? trctblsize : TRACE_TABLE_SIZE_DEFAULT)) /* assignment! */
				{
					if (TRACE_TABLE_SIZE_MAX < trctblsize)
						trctblsize = TRACE_TABLE_SIZE_MAX;
					trctblbytes = trctblsize * SIZEOF(trctbl_entry);
					TREF(gtm_trctbl_start) = malloc(trctblbytes);
					TREF(gtm_trctbl_end) = TREF(gtm_trctbl_start) + trctblsize;
					TREF(gtm_trctbl_cur) = TREF(gtm_trctbl_start) - 1; /* So doesn't skip 1st entry */
					memset(TREF(gtm_trctbl_start), 0, trctblbytes);
				}
			}
		}
#		endif
		/* Initialize jnl_extract_nocol */
		TREF(jnl_extract_nocol) = ydb_trans_numeric(YDBENVINDX_EXTRACT_NOCOL, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		/* Initialize dollar_zmaxtptime */
		status = ydb_trans_numeric(YDBENVINDX_MAXTPTIME, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		if (is_defined && (0 <= status) && (TPTIMEOUT_MAX_TIME >= status))
			TREF(dollar_zmaxtptime) = status;
		/* See if $ydb_ztrap_new has been specified */
		ztrap_new = ydb_logical_truth_value(YDBENVINDX_ZTRAP_NEW, FALSE, NULL);
		/* See if $ydb_malloc_limit is set. Initialize dollar_zmalloclim and malloccrit_issued */
		zmalloclim = ydb_trans_numeric_64(YDBENVINDX_MALLOC_LIMIT, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		tmp_malloc_limit = (size_t)getstorage();
		if (!is_defined || IS_GTMSECSHR_IMAGE)
		{
			zmalloclim = 0;					/* default is 0; exclude gtmsecshr */
			malloccrit_issued = TRUE;
		}
		else if (0 > zmalloclim)				/* negative gives half the OS limit */
			zmalloclim = tmp_malloc_limit / 2;		/* see gtm_malloc_src.h MALLOC macro comment on halving */
		else if (zmalloclim > tmp_malloc_limit)
			zmalloclim = tmp_malloc_limit;
		else if (zmalloclim < MIN_MALLOC_LIM)
			zmalloclim = MIN_MALLOC_LIM;
		/* See if $ydb_mupjnl_parallel is set */
		ydb_mupjnl_parallel = ydb_trans_numeric(YDBENVINDX_MUPJNL_PARALLEL, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		if (!is_defined)
			ydb_mupjnl_parallel = 1;
		/* See if ydb_repl_filter_timeout is specified */
		ydb_repl_filter_timeout = ydb_trans_numeric(YDBENVINDX_REPL_FILTER_TIMEOUT, &is_defined, IGNORE_ERRORS_TRUE, NULL);
		if (!is_defined)
			ydb_repl_filter_timeout = REPL_FILTER_TIMEOUT_DEF;
		else if (REPL_FILTER_TIMEOUT_MIN > ydb_repl_filter_timeout)
			ydb_repl_filter_timeout = REPL_FILTER_TIMEOUT_MIN;
		else if (REPL_FILTER_TIMEOUT_MAX < ydb_repl_filter_timeout)
			ydb_repl_filter_timeout = REPL_FILTER_TIMEOUT_MAX;
		assert((REPL_FILTER_TIMEOUT_MIN <= ydb_repl_filter_timeout)
				&& (REPL_FILTER_TIMEOUT_MAX >= ydb_repl_filter_timeout));
		ret = ydb_logical_truth_value(YDBENVINDX_DOLLAR_TEST, FALSE, &is_defined);
		dollar_test_default = (is_defined ? ret : TRUE);
		/* gtm_nofflf for GTM-9136.  Default is FALSE */
		gtm_nofflf = ydb_logical_truth_value(YDBENVINDX_NOFFLF, FALSE, &is_defined);
		/* Platform specific initializations */
		gtm_env_init_sp();
	}
}
