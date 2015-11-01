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
#include "zbreak.h"

void zr_init(z_records *z, int4 count, int4 rec_size)
{
	short size;

	size =  count * rec_size;
	z->beg = malloc(size);
	z->free = z->beg;
	z->end = z->beg + size;
	z->rec_size = rec_size;
}
