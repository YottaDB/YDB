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
#include "mlk_pvtblk_delete.h"

void mlk_pvtblk_delete(mlk_pvtblk **prior)
{
	mlk_pvtblk *delete;

	delete = (*prior);
	(*prior) = delete->next;
	free (delete);
}
