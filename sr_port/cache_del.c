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

#include "mdq.h"
#include "cache.h"

GBLREF cache_entry	*cache_hashent;
GBLREF int		cache_hits, cache_fails;

void cache_del (unsigned char code, mstr *source, mstr *object)
{
	cache_entry	*cp;

	cache_hash(code, source);	/* set cache_hashent to hash bucket for desired entry */
	dqloop(cache_hashent, linkq, cp)
	{
		if (source->len == cp->src.len
		    && code  == cp->code
		    && (source->len == 0  ||  memcmp(source->addr, cp->src.addr, source->len) == 0))
		{
			assert (object->len == cp->obj.len);
			assert (object->len == 0  ||  memcmp(object->addr, cp->obj.addr, object->len) == 0);

			cache_hits++;
			dqdel (cp, linkq);			/* delete from cache_hashent list */
			/* Only thing we did was remove it from cache so can no longer be found. It still
			   must become "unused" in order to be reused. */
			return;
		}
	}
	cache_fails++;
	GTMASSERT;	/* Could not find entry to delete */
}
