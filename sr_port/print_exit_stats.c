/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Common area to put all printing of stats on exit */
#include "mdef.h"

#include "gtmdbglvl.h"
#include "print_exit_stats.h"
#include "fnpc.h"
#include "cache.h"
#include "sockint_stats.h"
#include "gtm_text_alloc.h"

GBLREF	uint4		gtmDebugLevel;		/* Debug level (0 = using default sm module so with
						   a DEBUG build, even level 0 implies basic debugging) */
void print_exit_stats(void)
{
	if ((GDL_SmStats | GDL_SmDumpTrace | GDL_SmDump) & gtmDebugLevel)
	{
		printMallocInfo();
#ifdef COMP_GTA
		printAllocInfo();	/* Print mmap stats if gtm_text_alloc.c was built */
#endif
	}
#ifdef DEBUG
	if (GDL_PrintPieceStats & gtmDebugLevel)
		fnpc_stats();
#endif
	if (GDL_PrintIndCacheStats & gtmDebugLevel)
		cache_stats();
	if (GDL_PrintSockIntStats & gtmDebugLevel)
		sockint_stats();
}
