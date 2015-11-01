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

#include "zbreak.h"

void zr_put_free(z_records *zptr, char *cptr)
{
	assert(zptr->beg <= zptr->end);
	assert(cptr >= zptr->beg);
	assert(cptr < zptr->end);
	memmove(cptr, cptr + zptr->rec_size, zptr->end - cptr);	/* as of 18 Oct 1999, we changed this to a memmove from a memcpy */
	zptr->free -= zptr->rec_size;
}
