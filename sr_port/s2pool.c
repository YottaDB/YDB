/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
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

void
s2pool(mstr *a)
{
	GBLREF spdesc stringpool;
	int al;

	if ((al = a->len) == 0)
		return;
	/* One would be tempted to check if "a" is already in the stringpool (using the "IS_IN_STRINGPOOL" macro)
	 * and if so, return right away. But this can cause issues in some callers like "get_frame_place_mcode()"
	 * which assert that "IS_AT_END_OF_STRINGPOOL()" is TRUE after a "s2pool()" call. Therefore, we do not return
	 * in this case but create another copy of "a" at the end of the stringpool and then return even if it is a duplicate.
	 */
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	ENSURE_STP_FREE_SPACE(al);
	memcpy(stringpool.free,a->addr,al);
	a->addr = (char *)stringpool.free;
	stringpool.free += al;
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
}
