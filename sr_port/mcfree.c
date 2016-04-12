/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "mmemory.h"

GBLREF int 		mcavail;
GBLREF mcalloc_hdr 	*mcavailptr, *mcavailbase;

/* This routine doesn't actually release memory at the end of compilation. For efficiency sake
 * (esp. during indirect code compilation), it just resets the mcavail* pointers to the beginning
 * of the list so that the allocated area will be reused for the next compilation.
 */
void mcfree(void)
{
	mcavailptr = mcavailbase;
	assert((NULL != mcavailptr) || !mcavail);
	mcavail = (NULL != mcavailptr) ? mcavailptr->size : 0;
	return;
}
