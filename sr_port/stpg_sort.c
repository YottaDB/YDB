/****************************************************************
 *								*
 * Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017-2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "stpg_sort.h"

typedef mstr* ref_mstr;
/* See https://github.com/swenson/sort#usage for why these macros are so defined here */
#define SORT_NAME sorter
#define SORT_TYPE ref_mstr
#define SORT_CMP(x, y)  ((x)->addr < (y)->addr ? -1 : ((y)->addr < (x)->addr ? 1 : 0))
#define SORT_CSWAP(x,y) {ref_mstr tmp = (x); (x) = (y); (y) = tmp;}

static SORT_TYPE *gstore = NULL;
static size_t gstore_size = 0;

#include "sort.h"
/*-------------------------------------------------------------------------*/

void stpg_sort (mstr **base, mstr **top)
{
    size_t size = top - base + 1;
    sorter_tim_sort(base,size);	/* this invokes the TIM_SORT macro function in sort.h */
}
