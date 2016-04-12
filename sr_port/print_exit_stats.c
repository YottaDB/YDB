/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_string.h"
#include "gtm_stdlib.h"

#include "gtmdbglvl.h"
#include "print_exit_stats.h"
#include "fnpc.h"
#include "cache.h"
#include "sockint_stats.h"
#include "pipeint_stats.h"
#include "gtm_malloc.h"
#include "gtm_text_alloc.h"
#include "mmemory.h"
#include "gtmio.h"
#include "have_crit.h"
#ifdef UNICODE_SUPPORTED
#include "utfcgr.h"
#endif

#ifdef AIX
# define PMAPSTR	"procmap "
#else
# define PMAPSTR	"pmap "
#endif


GBLREF	uint4		gtmDebugLevel;		/* Debug level (0 = using default sm module so with
						 * a DEBUG build, even level 0 implies basic debugging).
						 */
GBLREF	boolean_t	gtm_utf8_mode;
GBLREF	mcalloc_hdr 	*mcavailptr, *mcavailbase;

void print_exit_stats(void)
{
	DBGMCALC_ONLY(int		mcblkcnt = 0;)
	DBGMCALC_ONLY(ssize_t		mcblktot = 0;)
	DBGMCALC_ONLY(mcalloc_hdr	*mcptr;)
	char				systembuff[64];
	char				*cmdptr;

	if ((GDL_SmStats | GDL_SmDumpTrace | GDL_SmDump) & gtmDebugLevel)
	{
		printMallocInfo();
#		ifdef COMP_GTA
		printAllocInfo();	/* Print mmap stats if gtm_text_alloc.c was built */
#		endif
	}
#	ifdef DEBUG
	if (GDL_PrintCacheStats & gtmDebugLevel)
	{
		fnpc_stats();
#		ifdef UNICODE_SUPPORTED
		if (gtm_utf8_mode)
			utfcgr_stats();
#		endif
	}
#	endif
	if (GDL_PrintIndCacheStats & gtmDebugLevel)
		cache_stats();
	if (GDL_PrintSockIntStats & gtmDebugLevel)
		sockint_stats();
	if (GDL_PrintPipeIntStats & gtmDebugLevel)
		pipeint_stats();
	if (GDL_PrintPMAPStats & gtmDebugLevel)
	{
		cmdptr = &systembuff[0];
		MEMCPY_LIT(cmdptr, PMAPSTR);
		cmdptr += STR_LIT_LEN(PMAPSTR);
		cmdptr = (char *)i2asc((uchar_ptr_t)cmdptr, getpid());
		*cmdptr = '\0';
		assert(cmdptr <= ARRAYTOP(systembuff));
		SYSTEM(systembuff);
	}
#	ifdef DEBUG_MCALC
	/* Find out how many plus total size of mcalloc() blocks exist and print the stats */
	for (mcptr = mcavailbase; mcptr; mcblkcnt++, mcblktot += mcptr->size, mcptr = mcptr->link);
	FPRINTF(stderr, "mcalloc() stats - Blocks: %d, Total size: %ld\n", mcblkcnt, mcblktot);
#	endif
}
