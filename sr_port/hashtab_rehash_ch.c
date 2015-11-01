/****************************************************************
 *								*
 *	Copyright 2003, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "error.h"
#include "hashtab_rehash_ch.h"

CONDITION_HANDLER(hashtab_rehash_ch)
{
	/* If we cannot alloc memory during rehasing, just continue in normal program flow */
	error_def(ERR_MEMORY);
	error_def(ERR_VMSMEMORY);
	error_def(ERR_MEMORYRECURSIVE);
	error_def(ERR_HTOFLOW);
	START_CH;
	/* If we cannot allocate memory or any error while doing rehash, just abort any more rehashing.
	 *  We will continue with old table */
	if (ERR_HTOFLOW == SIGNAL || ERR_MEMORY == SIGNAL || ERR_VMSMEMORY == SIGNAL || ERR_MEMORYRECURSIVE == SIGNAL)
	{
		UNWIND(NULL, NULL);
	}
	else
	{
		NEXTCH; /* non memory related error */
	}
}
