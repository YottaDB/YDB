/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gcol_list.h"

void
s2pool(mstr *a)
{
	mstr_len_t al;

	if ((al = a->len) == 0) /* WARNING - assignment */
		return;
	/* TODO: assert(glist_str_protected(a)); Some special pre-existing cases currently prevent this assert. */
	if (IS_IN_STRINGPOOL(a->addr, al))
	{
		/* assert(FALSE); */
		return;
	}
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	ENSURE_STP_FREE_SPACE(al);
	memcpy(stringpool.free, a->addr, al);
	a->addr = (char *)stringpool.free;
	stringpool.free += al;
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
}
