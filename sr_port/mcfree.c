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

#include "mmemory.h"

GBLREF int mcavail;
GBLREF char **mcavailptr, **mcavailbase;

void mcfree(void)
{
	mcavailptr = mcavailbase;
	mcavail = MC_DSBLKSIZE - sizeof(char_ptr_t);
	return;
}
