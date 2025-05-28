/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

GBLREF spdesc 		stringpool;

void s2pool_align(mstr *string)
{
	int length;

	if ((length = string->len) == 0)
		return;
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	ENSURE_STP_FREE_SPACE(length);
	memcpy(stringpool.free, string->addr, length);
	string->addr = (char *)stringpool.free;
	stringpool.free += length;
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
}
