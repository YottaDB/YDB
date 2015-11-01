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

/*
 * -----------------------------------------------
 * This routine is a one-argument front-end for op_zdeallocate
 *  which has a second argument to deal with secondary ownership,
 *  such as that used by a server on behalf of its clients.
 * -----------------------------------------------
 */

#include "mdef.h"
#include "op.h"

void op_zdeallocate (int4 timeout)
{
	op_zdealloc2(timeout, 0);
	return;
}
