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

#include "mmemory.h"

#define MC_DSBLKSIZE 8180

GBLDEF char **mcavailptr, **mcavailbase;
GBLDEF int mcavail;

char *mcalloc(unsigned int n)
{
	char **x;

	n = ((n + 3) & ~3);
	if (n > mcavail)
	{
		if (*mcavailptr)
			mcavailptr = (char ** ) *mcavailptr;
		else
		{
			x = (char **) malloc(MC_DSBLKSIZE);
			*mcavailptr = (char *) x;
			mcavailptr = x;
			*mcavailptr = 0;
		}
		mcavail = MC_DSBLKSIZE - sizeof(char *);
		memset(mcavailptr + 1, 0, mcavail);
	}
	mcavail -= n;
	assert(mcavail >= 0);
	return (char *) mcavailptr + mcavail + sizeof(char *);
}
