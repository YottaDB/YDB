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

#include "mdef.h"

#include "gtm_string.h"

#include "cache.h"
#include "copy.h"

GBLREF cache_tabent	*cache_tabent_base;
GBLREF cache_entry	*cache_hashent;

/* cache_hash - set cache_hashent to the hash bucket for the specified entry.  */

void	cache_hash(unsigned char code, mstr *source)
{
	int	size;
	int4	result;
	int4	val1, val2;

	result = code;
	size = (int)source->len;
	if (0 < size)
	{
		if (size < sizeof(int4))
		{
			val1 = 0;	/* make sure no stray characters left over */
			memcpy ((char *)&val1, source->addr, size);
			result ^= val1;
		} else
		{
			GET_LONG(val1, source->addr);
			GET_LONG(val2, source->addr + size - sizeof(int4));
			result ^= val1 * 2 + val2;
		}
	}
	cache_hashent = (cache_entry *)(cache_tabent_base + ((result & MAXPOSINT4) % CACHE_TAB_SIZE));
	return;
}
