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

/* cachectl.h - values used as parameters to cacheflush
 *
 *	ICACHE - flush instruction cache
 *	DCACHE - flush data cache
 *	BCACHE - flush both caches
 */


#define ICACHE	0x1
#define DCACHE	0x2
#define BCACHE	(ICACHE|DCACHE)
