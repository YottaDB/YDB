/****************************************************************
 *								*
 *	Copyright 2004 Sanchez Computer Associates, Inc.	*
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
#include "gtm_env_init.h"	/* for gtm_env_init() and gtm_env_init_sp() prototype */

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

GBLREF	boolean_t	gvdupsetnoop; 		/* if TRUE, duplicate SETs update journal but not database (except
						   for curr_tn++) */
GBLREF	uint4		gtmDebugLevel; 		/* Debug level (0 = using default sm module so with
						   a DEBUG build, even level 0 implies basic debugging) */
GBLREF	boolean_t	gtm_fullblockwrites;	/* Do full (not partial) database block writes T/F */

void	gtm_env_init(void)
{
	static boolean_t	gtm_env_init_done = FALSE;
	mstr			val;
	boolean_t		ret, is_defined;
	uint4			tdbglvl;

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
		ret = logical_truth_value(&val, &is_defined);
		if (is_defined)
			gvdupsetnoop = ret; /* if the logical is not defined, we want gvdupsetnoop to take its default value */

		/* Full Database-block Write mode */
		val.addr = GTM_FULLBLOCKWRITES;
		val.len = sizeof(GTM_FULLBLOCKWRITES) - 1;
		gtm_fullblockwrites = logical_truth_value(&val, &is_defined);
		if (!is_defined)
			gtm_fullblockwrites = DEFAULT_FBW_FLAG;

		/* Platform specific initializations */
		gtm_env_init_sp();
		gtm_env_init_done = TRUE;
	}
}
