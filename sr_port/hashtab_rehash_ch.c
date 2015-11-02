/****************************************************************
 *								*
 *	Copyright 2003, 2010 Fidelity Information Services, Inc	*
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

CONDITION_HANDLER(hashtab_rehash_ch)
{
	/* If we cannot alloc memory during rehashing, just continue in normal program flow */
	error_def(ERR_MEMORY);
	error_def(ERR_MEMORYRECURSIVE);
	error_def(ERR_HTOFLOW);

	START_CH;
	/* If we cannot allocate memory or any error while doing rehash, just abort any more rehashing.
	 *  We will continue with old table. Note that we do not ignore VMSMEMORY errors because if a
	 *  VMS_MEMORY error occurred, gtm_malloc is going to have released the memory cache trying to get
	 *  through process exit cleanly. We have no option to ignore the memory error on VMS
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
