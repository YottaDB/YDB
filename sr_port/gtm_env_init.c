/****************************************************************
 *								*
 *	Copyright 2004, 2011 Fidelity Information Services, Inc	*
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
#include "logical_truth_value.h"
#include "trans_numeric.h"
#include "trans_log_name.h"
#include "gtmdbglvl.h"
#include "iosp.h"
#include "wbox_test_init.h"
#include "gtm_env_init.h"	/* for gtm_env_init() and gtm_env_init_sp() prototype */
#include "gt_timer.h"
#include "io.h"
#include "iotcpdef.h"
#include "iosocketdef.h"
#include "gtm_malloc.h"
#include "cache.h"
#include "gdsroot.h"		/* needed for gdsfhead.h */
#include "gdskill.h"		/* needed for gdsfhead.h */
#include "gdsbt.h"		/* needed for gdsfhead.h */
#include "gdsfhead.h"		/* needed for MAXTOTALBLKS_MAX macro */
#include "mvalconv.h"
#include "fullbool.h"

#ifdef DEBUG
#  define INITIAL_DEBUG_LEVEL GDL_Simple
#else
#  define INITIAL_DEBUG_LEVEL GDL_None
#endif

#ifdef FULLBLOCKWRITES
#  define DEFAULT_FBW_FLAG TRUE
#else
#  define DEFAULT_FBW_FLAG FALSE
#endif
#define SIZEOF_prombuf ggl_prombuf

GBLREF	boolean_t	dollar_zquit_anyway;	/* if TRUE compile QUITs to not care whether or not they're from an extrinsic */
GBLREF	boolean_t	gvdupsetnoop; 		/* if TRUE, duplicate SETs update journal but not database (except
						   for curr_tn++) */
GBLREF	uint4		gtmDebugLevel; 		/* Debug level (0 = using default sm module so with
						   a DEBUG build, even level 0 implies basic debugging) */
GBLREF	boolean_t	gtm_fullblockwrites;	/* Do full (not partial) database block writes T/F */
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

void	gtm_env_init(void)
{
	mstr			val, trans;
	boolean_t		ret, is_defined;
	uint4			tdbglvl, tmsock, reservesize, memsize, cachent;
	int4			status;
	char			buf[MAX_TRANS_NAME_LEN];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!TREF(gtm_env_init_done))
	{
		/* See if a debug level has been specified. Do this first since gtmDebugLevel needs
		   to be initialized before any mallocs are done in the system.
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
		/* Duplicate Set Noop environment/logical */
		val.addr = GTM_GVDUPSETNOOP;
		val.len = SIZEOF(GTM_GVDUPSETNOOP) - 1;
		assert(FALSE == gvdupsetnoop);	/* should have been set to FALSE in gbldefs.c */
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			gvdupsetnoop = ret; /* if the logical is not defined, we want gvdupsetnoop to take its default value */
		/* gtm_boolean environment/logical */
		val.addr = GTM_BOOLEAN;
		val.len = SIZEOF(GTM_BOOLEAN) - 1;
		TREF(gtm_fullbool) = trans_numeric(&val, &is_defined, TRUE);
		/* NOUNDEF environment/logical */
		val.addr = GTM_NOUNDEF;
		val.len = SIZEOF(GTM_NOUNDEF) - 1;
		assert(FALSE == undef_inhibit);	/* should have been set to FALSE at global variable definition time */
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			undef_inhibit = ret; /* if the logical is not defined, we want undef_inhibit to take its default value */
#		ifdef DEBUG
		/* GTM_GVUNDEF_FATAL environment/logical */
		val.addr = GTM_GVUNDEF_FATAL;
		val.len = SIZEOF(GTM_GVUNDEF_FATAL) - 1;
		assert(FALSE == TREF(gtm_gvundef_fatal));	/* should have been set to FALSE by gtm_threadgbl_defs */
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			TREF(gtm_gvundef_fatal) = ret; /* if logical is not defined, gtm_gvundef_fatal takes the default value */
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
		gtm_fullblockwrites = logical_truth_value(&val, FALSE, &is_defined);
		if (!is_defined)
			gtm_fullblockwrites = DEFAULT_FBW_FLAG;
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
		if (GETENV("gtm_environment_init"))
		{
			TREF(gtm_environment_init) = TRUE;
		}
		/* Platform specific initializations */
		gtm_env_init_sp();
		TREF(gtm_env_init_done) = TRUE;
	}
}
