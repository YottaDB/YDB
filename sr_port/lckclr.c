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
#include "mlkdef.h"
#include "lckclr.h"

GBLREF short lks_this_cmd;
GBLREF mlk_pvtblk *mlk_pvt_root;

void lckclr(void)
{
	short i;
	mlk_pvtblk *p1;

	p1 = mlk_pvt_root;
	for (i = 0; i < lks_this_cmd; i++)
	{
		p1->trans = 0;
		p1 = p1->next;
	}
}
