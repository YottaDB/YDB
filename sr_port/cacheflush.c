/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* cacheflush stub
 *
 * Most hardware platforms on which GT.M is implemented use separate
 * instruction and data caches.  It is necessary to flush these caches
 * whenever we generate code in a data region in order to make sure
 * the generated code gets written from the data cache to memory and
 * subsequently loaded from memory into the instruction cache.
 *
 * Input:	addr		starting address of region to flush
 *		nbytes		size, in bytes, of region to flush
 *		cache_select	flag indicating whether to flus
 *				I-cache, D-cache, or both
 *
 * This stub is for those platforms that don't use separate data and
 * instruction caches.
 */

#include "mdef.h"
#include "cacheflush.h"


int	cacheflush (void *addr, long nbytes, int cache_select)
{
	return 0;	/* incr_link requires a zero return value for success */
}
