/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Common area to put all printing of stats on exit */
#include "mdef.h"

#include "gtm_stdio.h"

#include "gtmdbglvl.h"
#include "print_exit_stats.h"
#include "fnpc.h"
#include "cache.h"
#include "sockint_stats.h"
#ifdef UNIX
#include "pipeint_stats.h"
#endif
#include "gtm_malloc.h"
#include "gtm_text_alloc.h"
#include "mmemory.h"
#include "gtmio.h"
#include "have_crit.h"

GBLREF	uint4		gtmDebugLevel;		/* Debug level (0 = using default sm module so with
						 * a DEBUG build, even level 0 implies basic debugging).
						 */
GBLREF	mcalloc_hdr 	*mcavailptr, *mcavailbase;

void print_exit_stats(void)
{
	DBGMCALC_ONLY(int		mcblkcnt = 0;)
	DBGMCALC_ONLY(ssize_t		mcblktot = 0;)
	DBGMCALC_ONLY(mcalloc_hdr	*mcptr;)

	if ((GDL_SmStats | GDL_SmDumpTrace | GDL_SmDump) & gtmDebugLevel)
	{
		printMallocInfo();
#		ifdef COMP_GTA
		printAllocInfo();	/* Print mmap stats if gtm_text_alloc.c was built */
#		endif
	}
#	ifdef DEBUG
	if (GDL_PrintPieceStats & gtmDebugLevel)
		fnpc_stats();
#	endif
	if (GDL_PrintIndCacheStats & gtmDebugLevel)
		cache_stats();
	if (GDL_PrintSockIntStats & gtmDebugLevel)
		sockint_stats();
#	ifdef UNIX
	if (GDL_PrintPipeIntStats & gtmDebugLevel)
		pipeint_stats();
#	endif
#	ifdef DEBUG_MCALC
	/* Find out how many plus total size of mcalloc() blocks exist and print the stats */
	for (mcptr = mcavailbase; mcptr; mcblkcnt++, mcblktot += mcptr->size, mcptr = mcptr->link);
	FPRINTF(stderr, "mcalloc() stats - Blocks: %d, Total size: %ld\n", mcblkcnt, mcblktot);
#	endif
}
