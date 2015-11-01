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

/* ionl_rdone.c */

#include "mdef.h"
#include "io.h"

short	ionl_rdone(mint *val, int4 timeout)
{
	mval	tmp;

	*val = -1;
	return ionl_readfl(&tmp, 1, timeout);
}
