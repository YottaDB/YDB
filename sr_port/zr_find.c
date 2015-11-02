/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

zbrk_struct *zr_find(z_records *zrecs, zb_code *addr)
{
	zbrk_struct *z_ptr;

	/* NOTE: records are stored by decreasing addresses */
	for (z_ptr = zrecs->beg; z_ptr < zrecs->free; z_ptr++)
	{
		if (z_ptr->mpc == addr)
			return(z_ptr);
	}
	return NULL;
}
