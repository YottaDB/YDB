/****************************************************************
 *								*
 * Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 * Copyright (c) 2017 Stephen L Johnson. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* STUB FILE only for non-ia64 and non-arm versions */
#include "mdef.h"

#include "inst_flush.h"
#include "cacheflush.h"

void inst_flush(void *start, int4 len)
{
        IA64_ONLY(cacheflush(start, len, 0 ));
	ARM_ONLY(cacheflush(start, len, 0 ));
}
