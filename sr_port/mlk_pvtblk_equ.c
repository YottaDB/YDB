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

#include "gtm_string.h"

#include "mlkdef.h"
#include "mlk_pvtblk_equ.h"

int mlk_pvtblk_equ(mlk_pvtblk *a, mlk_pvtblk *b)
{
	return (a->ctlptr == b->ctlptr && a->total_length == b->total_length
		&& (memcmp(a->value, b->value, a->total_length) == 0));
}
