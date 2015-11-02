/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <rtnhdr.h>
#include "zbreak.h"

void zr_init(z_records *zrecs, int4 count)
{
	zrecs->beg = (zbrk_struct *)malloc(count * SIZEOF(zbrk_struct));
	zrecs->free = zrecs->beg;
	zrecs->end = zrecs->beg + count;
	return;
}
