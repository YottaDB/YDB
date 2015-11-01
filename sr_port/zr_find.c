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

char *zr_find(z_records *z, char *addr)
{
	char *c;

	/* NOTE: records are stored by decreasing addresses */
	for (c = z->beg ;c <= z->free ;c += z->rec_size)
	{
		if (*(int4*)c == (int) addr)
			return(c);
	}
	return((char*)0);
}
