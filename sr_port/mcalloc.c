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

#include "gtm_string.h"
#include "mmemory.h"

GBLDEF char **mcavailptr, **mcavailbase;
GBLDEF int mcavail;

char *mcalloc(unsigned int n)
{
	char **x;

	/* Choice of char_ptr_t made because it is a 64 bit pointer on Tru64 which
	   is the alignment we need there, or any other 64 bit platforms we support
	   in the future.
	*/
	n = ROUND_UP2(n, sizeof(char_ptr_t));
	if (n > mcavail)
	{
		if (*mcavailptr)
			mcavailptr = (char ** ) *mcavailptr;
		else
		{
			x = (char **)malloc(MC_DSBLKSIZE);
			*mcavailptr = (char *) x;
			mcavailptr = x;
			*mcavailptr = 0;
		}
		mcavail = MC_DSBLKSIZE - sizeof(char_ptr_t);
		memset(((char *)mcavailptr) + sizeof(char_ptr_t), 0, mcavail);
	}
	mcavail -= n;
	assert(mcavail >= 0);
	return (char *)mcavailptr + mcavail + sizeof(char_ptr_t);
}
