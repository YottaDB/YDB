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

#include "mdq.h"
#include "cache.h"

GBLREF cache_entry	*cache_hashent;
GBLREF int		cache_hits, cache_fails;

/* cache_get - get cached indirect object code corresponding to input source and code.
 *
 *	If object code exists in cache, return pointer to object code mstr
 *	otherwise, return 0.
 *
 *	In either case, set cache_hashent (via cache_hash) to the hash bucket for the
 *	desired entry.
 */

mstr	*cache_get(unsigned char code, mstr *source)
{
	cache_entry	*cp;

	cache_hash(code, source);	/* set cache_hashent to hash bucket corresponding to desired entry */
	dqloop(cache_hashent, linkq, cp)
	{
		if (source->len == cp->src.len
		    && code == cp->code
                    && (0 == source->len || 0 == memcmp(source->addr, cp->src.addr, source->len)))
		{
			cache_hits++;
			return &cp->obj;
		}
	}

	cache_fails++;
	return NULL;
}
