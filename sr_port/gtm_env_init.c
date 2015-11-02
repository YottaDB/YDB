/****************************************************************
 *								*
 *	Copyright 2004, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_logicals.h"
#include "logical_truth_value.h"
#include "trans_numeric.h"
#include "gtmdbglvl.h"
#include "iosp.h"
#include "wbox_test_init.h"
#include "gtm_env_init.h"	/* for gtm_env_init() and gtm_env_init_sp() prototype */
#include "collseq.h"
#include "gt_timer.h"
#include "io.h"
#include "iotcpdef.h"
#include "iosocketdef.h"
#include "gtm_malloc.h"
#include "cache.h"

#ifdef DEBUG
#  define INITIAL_DEBUG_LEVEL GDL_Simple
GBLDEF	boolean_t 	gtmdbglvl_inited;	/* gtmDebugLevel has been initialized */
#else
#  define INITIAL_DEBUG_LEVEL GDL_None
#endif

#ifdef FULLBLOCKWRITES
#  define DEFAULT_FBW_FLAG TRUE
#else
#  define DEFAULT_FBW_FLAG FALSE
#endif

GBLREF	boolean_t	dollar_zquit_anyway;	/* if TRUE compile QUITs to not care whether or not they're from an extrinsic */
GBLREF	boolean_t	gvdupsetnoop; 		/* if TRUE, duplicate SETs update journal but not database (except
						   for curr_tn++) */
GBLREF	uint4		gtmDebugLevel; 		/* Debug level (0 = using default sm module so with
						   a DEBUG build, even level 0 implies basic debugging) */
GBLREF	boolean_t	gtm_fullblockwrites;	/* Do full (not partial) database block writes T/F */
GBLREF	boolean_t	certify_all_blocks;
GBLREF	boolean_t	local_collseq_stdnull;  /* If true, standard null subscript collation will be used for local variables */
GBLREF	uint4		gtm_blkupgrade_flag;	/* controls whether dynamic block upgrade is attempted or not */
GBLREF	boolean_t	gtm_dbfilext_syslog_disable;	/* control whether db file extension message is logged or not */
GBLREF	uint4		gtm_max_sockets;	/* Maximum sockets in a socket device that can be created by this process */
GBLREF	bool		undef_inhibit;
GBLREF	uint4		outOfMemoryMitigateSize;	/* Reserve that we will freed to help cleanup if run out of memory */
GBLREF	uint4		max_cache_memsize;	/* Maximum bytes used for indirect cache object code */
GBLREF	uint4		max_cache_entries;	/* Maximum number of cached indirect compilations */

void	gtm_env_init(void)
{
	static boolean_t	gtm_env_init_done = FALSE;
	mstr			val;
	boolean_t		ret, is_defined;
	uint4			tdbglvl, tmsock, reservesize, memsize, cachent;

	if (!gtm_env_init_done)
	{
		/* See if a debug level has been specified. Do this first since gtmDebugLevel needs
		   to be initialized before any mallocs are done in the system.
		*/
		gtmDebugLevel = INITIAL_DEBUG_LEVEL;
		val.addr = GTM_DEBUG_LEVEL_ENVLOG;
		val.len = sizeof(GTM_DEBUG_LEVEL_ENVLOG) - 1;
		if (tdbglvl = trans_numeric(&val, &is_defined, TRUE)) /* Note assignment!! */
		{	/* Some kind of debugging was asked for.. */
			tdbglvl |= GDL_Simple;			/* Make sure simple debugging turned on if any is */
			if ((GDL_SmChkFreeBackfill | GDL_SmChkAllocBackfill) & tdbglvl)
				tdbglvl |= GDL_SmBackfill;	/* Can't check it unless it's filled in */
			if (GDL_SmStorHog & tdbglvl)
				tdbglvl |= GDL_SmBackfill | GDL_SmChkAllocBackfill;
			gtmDebugLevel |= tdbglvl;
		}
		DEBUG_ONLY(gtmdbglvl_inited = TRUE);

		/* Duplicate Set Noop environgment/logical */
		val.addr = GTM_GVDUPSETNOOP;
		val.len = sizeof(GTM_GVDUPSETNOOP) - 1;
		assert(FALSE == gvdupsetnoop);	/* should have been set to FALSE in gbldefs.c */
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			gvdupsetnoop = ret; /* if the logical is not defined, we want gvdupsetnoop to take its default value */

		/* NOUNDEF environgment/logical */
		val.addr = GTM_NOUNDEF;
		val.len = sizeof(GTM_NOUNDEF) - 1;
		assert(FALSE == undef_inhibit);	/* should have been set to FALSE in gbldefs.c */
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			undef_inhibit = ret; /* if the logical is not defined, we want undef_inhibit to take its default value */

		/* Full Database-block Write mode */
		val.addr = GTM_FULLBLOCKWRITES;
		val.len = sizeof(GTM_FULLBLOCKWRITES) - 1;
		gtm_fullblockwrites = logical_truth_value(&val, FALSE, &is_defined);
		if (!is_defined)
			gtm_fullblockwrites = DEFAULT_FBW_FLAG;

		/* GDS Block certification */
		val.addr = GTM_GDSCERT;
		val.len = sizeof(GTM_GDSCERT) - 1;
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			certify_all_blocks = ret; /* if the logical is not defined, we want to take default value */

		/* Initialize null subscript's collation order */
		val.addr = LCT_STDNULL;
		val.len = sizeof(LCT_STDNULL) - 1;
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			local_collseq_stdnull = ret;

		/* Initialize variables for white box testing */
		DEBUG_ONLY(wbox_test_init();)

		/* Initialize variable that controls dynamic GT.M block upgrade */
		val.addr = GTM_BLKUPGRADE_FLAG;
		val.len = sizeof(GTM_BLKUPGRADE_FLAG) - 1;
		gtm_blkupgrade_flag = trans_numeric(&val, &is_defined, TRUE);

		/* Initialize whether database file extensions need to be logged in the operator log */
		val.addr = GTM_DBFILEXT_SYSLOG_DISABLE;
		val.len = sizeof(GTM_DBFILEXT_SYSLOG_DISABLE) - 1;
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			gtm_dbfilext_syslog_disable = ret; /* if the logical is not defined, we want to take default value */

		/* Initialize maximum sockets in a single socket device createable by this process */
		gtm_max_sockets = MAX_N_SOCKET;
		val.addr = GTM_MAX_SOCKETS;
		val.len = sizeof(GTM_MAX_SOCKETS) - 1;
                if ((tmsock = trans_numeric(&val, &is_defined, TRUE)) && MAX_MAX_N_SOCKET > tmsock) /* Note assignment!! */
			gtm_max_sockets = tmsock;

		/* Initialize storage to allocate and keep in our back pocket in case run out of memory */
		outOfMemoryMitigateSize = GTM_MEMORY_RESERVE_DEFAULT;
		val.addr = GTM_MEMORY_RESERVE;
		val.len = sizeof(GTM_MEMORY_RESERVE) - 1;
		if (reservesize = trans_numeric(&val, &is_defined, TRUE)) /* Note assignment!! */
			outOfMemoryMitigateSize = reservesize;

		/* Initialize indirect cache limits (max memory, max entries) */
		max_cache_memsize = MAX_CACHE_MEMSIZE * 1024;
		val.addr = GTM_MAX_INDRCACHE_MEMORY;
		val.len = sizeof(GTM_MAX_INDRCACHE_MEMORY) - 1;
		if (memsize = trans_numeric(&val, &is_defined, TRUE)) /* Note assignment!! */
			max_cache_memsize = memsize * 1024;
		max_cache_entries = MAX_CACHE_ENTRIES;
                val.addr = GTM_MAX_INDRCACHE_COUNT;
                val.len = sizeof(GTM_MAX_INDRCACHE_COUNT) - 1;
                if (cachent = trans_numeric(&val, &is_defined, TRUE)) /* Note assignment!! */
                        max_cache_entries = cachent;

		/* Initialize ZQUIT to control funky QUIT compilation */
		val.addr = GTM_ZQUIT_ANYWAY;
		val.len = sizeof(GTM_ZQUIT_ANYWAY) - 1;
		ret = logical_truth_value(&val, FALSE, &is_defined);
		if (is_defined)
			dollar_zquit_anyway = ret;

		/* Platform specific initializations */
		gtm_env_init_sp();
		gtm_env_init_done = TRUE;
	}
}
