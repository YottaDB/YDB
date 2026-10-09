/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2019-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mlkdef.h"
#include "lckclr.h"

GBLREF	unsigned short	lks_this_cmd;
GBLREF	mlk_pvtblk	*mlk_pvt_root;

void lckclr(void)
{
	unsigned short i;
	mlk_pvtblk *p1;

	p1 = mlk_pvt_root;
	/* The list can hold fewer than lks_this_cmd entries when a condition handler calls this function after an error
	 * interrupted op_decrlock() or op_zdealloc2(), as they delete the current command's entries from the list one at a
	 * time without decrementing lks_this_cmd. Hence the NULL check.
	 */
	for (i = 0; (i < lks_this_cmd) && (NULL != p1); i++)
	{
		p1->trans = 0;
		p1 = p1->next;
	}
}
