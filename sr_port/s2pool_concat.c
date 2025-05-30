/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "stringpool.h"

void s2pool_concat(mval *dst, mstr *a)
{
	int	alen, dstlen;
	char	*dstaddr;

	if ((alen = a->len) == 0)
		return;
	assert(MV_STR == dst->mvtype);
	dstaddr = dst->str.addr;
	dstlen = dst->str.len;
	assert(IS_IN_STRINGPOOL(dstaddr, dstlen));
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	if (IS_AT_END_OF_STRINGPOOL(dstaddr, dstlen))
	{
		ENSURE_STP_FREE_SPACE(alen);
		memcpy(stringpool.free, a->addr, alen);
		stringpool.free += alen;
	} else
	{
		ENSURE_STP_FREE_SPACE(dstlen + alen);
		/* Note: The above macro could update "dst->str.addr" if "stp_gcol()" was invoked.
		 * In that case, "dstaddr" would still be pointing to the pre-stp_gcol() stringpool
		 * and so cannot be assumed to be valid anymore. Hence the "dst->str.addr" use below.
		 */
		memcpy(stringpool.free, dst->str.addr, dstlen);
		memcpy(stringpool.free + dstlen, a->addr, alen);
		dst->str.addr = (char *)stringpool.free;
		stringpool.free += dstlen + alen;
	}
	dst->str.len += alen;
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
}
