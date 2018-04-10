/****************************************************************
 *								*
 * Copyright (c) 2003-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "error.h"
#include "util.h"

error_def(ERR_MEMORY);
error_def(ERR_MEMORYRECURSIVE);
error_def(ERR_HTOFLOW);

CONDITION_HANDLER(hashtab_rehash_ch)
{
	/* If we cannot alloc memory during rehashing, just continue in normal program flow */
	START_CH(TRUE);
	/* If we cannot allocate memory or any error while doing rehash, just abort any more rehashing.
	 *  We will continue with old table.
	 */
	if (ERR_HTOFLOW == SIGNAL || ERR_MEMORY == SIGNAL || ERR_MEMORYRECURSIVE == SIGNAL)
	{
		UNIX_ONLY(util_out_print("", RESET));	/* Prevents error message from being flushed later by rts_error() */
		UNWIND(NULL, NULL);
	} else
	{
		NEXTCH; /* non memory related error */
	}
}
