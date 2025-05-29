/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include "stringpool.h"
#include "stp_parms.h"

GBLREF spdesc	stringpool;
OS_PAGE_SIZE_DECLARE

static unsigned char	*lasttop = 0;

void stp_init(size_t size)
{
	/* Allocate the size requested plus one longword so that loops that index through the stringpool can go one
	 * iteration beyond the end of the stringpool without running off the end of the allocated memory region.
	 */
	size = ROUND_UP2(size, 8);
	stringpool.base = stringpool.free = (unsigned char *)malloc(size);
	stringpool.lasttop = lasttop;
	lasttop = stringpool.top = stringpool.invokestpgcollevel = stringpool.base + size;
	stringpool.gcols = 0;
	return;
}
