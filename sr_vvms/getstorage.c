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
#include <jpidef.h>
#include "getstorage.h"

int4	getstorage(void)
{
	int4 status;
	uint4 page_count;
	status = lib$getjpi(&JPI$_FREPTECNT,0,0,&page_count,0,0);
	if ((status & 1) == 0)
		rts_error(status);
	return (int4)(page_count < (MAXPOSINT4 / OS_PAGELET_SIZE) ? (page_count * OS_PAGELET_SIZE) : MAXPOSINT4);
}
